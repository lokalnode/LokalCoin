// Copyright (c) 2014-2019 The Dash Core developers
// Copyright (c) 2021 The Lokal Coin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <masternode/activemasternode.h>
#include <consensus/validation.h>
#include <generation.h>
#include <governance/governance-classes.h>
#include <init.h>
#include <masternode/masternode-payments.h>
#include <masternode/masternode-sync.h>
#include <messagesigner.h>
#include <netfulfilledman.h>
#include <netmessagemaker.h>
#include <spork.h>
#include <util.h>
#include <validation.h>

#include <evo/deterministicmns.h>

#include <string>

CMasternodePayments mnpayments;

/**
* IsBlockValueValid
*
*   Determine if coinbase outgoing created money is the correct value
*
*   Why is this needed?
*   - In LOKAL_Coin some blocks are superblocks, which output much higher amounts of coins
*   - Otherblocks are 10% lower in outgoing value, so in total, no extra coins are created
*   - When non-superblocks are detected, the normal schedule should be maintained
*/

bool IsBlockValueValid(const CBlock& block, int nBlockHeight, CAmount expectedReward, CAmount actualReward, std::string& strErrorRet)
{
    strErrorRet = "";

    //! exception for generation block
    if (isGenerationBlock(nBlockHeight))
        return true;

    bool isProofOfStake = block.IsProofOfStake();
    const auto& coinbaseTransaction = block.vtx[isProofOfStake];
    bool isBlockRewardValueMet = (actualReward <= expectedReward);

    LogPrint(BCLog::MNPAYMENTS, "%s: actualReward %lld <= expectedReward %lld\n", __func__, actualReward, expectedReward);

    CAmount nSuperblockMaxValue =  expectedReward  + CSuperblock::GetPaymentsLimit(nBlockHeight);
    bool isSuperblockMaxValueMet = (actualReward <= nSuperblockMaxValue);

    LogPrint(BCLog::GOBJECT, "actualReward  %lld <= nSuperblockMaxValue %lld\n", block.vtx[0]->GetValueOut(), nSuperblockMaxValue);

    if (!CSuperblock::IsValidBlockHeight(nBlockHeight)) {
        // can't possibly be a superblock, so lets just check for block reward limits
        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, only regular blocks are allowed at this height",
                                    nBlockHeight, actualReward, nSuperblockMaxValue);
        }
        return isBlockRewardValueMet;
    }

    // bail out in case superblock limits were exceeded
    if (!isSuperblockMaxValueMet) {
        strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded superblock max value",
                                nBlockHeight, actualReward, nSuperblockMaxValue);
        return false;
    }

    // we are synced and possibly on a superblock now
    if (!sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED)) {
        // should NOT allow superblocks at all, when superblocks are disabled
        // revert to block reward limits in this case
        LogPrint(BCLog::GOBJECT, "%s -- Superblocks are disabled, no superblocks allowed\n", __func__);
        if(!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, superblocks are disabled",
                                    nBlockHeight, actualReward, expectedReward);
        }
        return isBlockRewardValueMet;
    }

    if (!CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
        // we are on a valid superblock height but a superblock was not triggered
        // revert to block reward limits in this case
        if(!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, no triggered superblock detected",
                                    nBlockHeight, actualReward, expectedReward);
        }
        return isBlockRewardValueMet;
    }

    // this actually also checks for correct payees and not only amount
    if (!CSuperblockManager::IsValid(*block.vtx[1], nBlockHeight, actualReward, expectedReward)) {
        // triggered but invalid? that's weird
        LogPrintf("%s -- ERROR: Invalid superblock detected at height %d: %s", __func__, nBlockHeight, block.vtx[10]->ToString());
        // should NOT allow invalid superblocks, when superblocks are enabled
        strErrorRet = strprintf("invalid superblock detected at height %d", nBlockHeight);
        return false;
    }

    // we got a valid superblock
    return true;
}

bool IsBlockPayeeValid(const CTransaction& txNew, int nBlockHeight, CAmount expectedReward, CAmount actualReward)
{
    /////////////////////////////////////////////////////////////////////////////////////////////////
    if (isGenerationBlock(nBlockHeight)) {
        LogPrintf("%s - %s\n", __func__, txNew.ToString());
        int correctRecipient = 0;
        for (const auto& l : txNew.vout) {
            if (isGenerationRecipient(HexStr(l.scriptPubKey))) {
                LogPrintf("      - %s - found correct recipient.. (generation block)\n", __func__);
                if (getGenerationAmount(nBlockHeight) == l.nValue) {
                    LogPrintf("      - %s - found correct amount.. (generation block)\n", __func__);
                    ++correctRecipient;
                }
            }
        }
        return (correctRecipient == 1);
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////

    if(fLiteMode) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        LogPrint(BCLog::MNPAYMENTS, "%s -- WARNING: Not enough data, skipping block payee checks\n", __func__);
        return true;
    }

    // we are still using budgets, but we have no data about them anymore,
    // we can only check masternode payments

    // superblocks started
    // SEE IF THIS IS A VALID SUPERBLOCK

    if (sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED)) {
        if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
            if (CSuperblockManager::IsValid(txNew, nBlockHeight, actualReward, expectedReward)) {
                LogPrint(BCLog::GOBJECT, "%s -- Valid superblock at height %d: %s", __func__, nBlockHeight, txNew.ToString());
                // continue validation, should also pay MN
            } else {
                LogPrintf("%s -- ERROR: Invalid superblock detected at height %d: %s", __func__, nBlockHeight, txNew.ToString());
                // should NOT allow such superblocks, when superblocks are enabled
                return false;
            }
        } else {
            LogPrint(BCLog::GOBJECT, "%s -- No triggered superblock detected at height %d\n", __func__, nBlockHeight);
        }
    } else {
        // should NOT allow superblocks at all, when superblocks are disabled
        LogPrint(BCLog::GOBJECT, "%s -- Superblocks are disabled, no superblocks allowed\n", __func__);
    }

    // Check for correct masternode payment
    if (mnpayments.IsTransactionValid(txNew, nBlockHeight, expectedReward)) {
        LogPrint(BCLog::MNPAYMENTS, "%s -- Valid masternode payment at height %d: %s", __func__, nBlockHeight, txNew.ToString());
        return true;
    }

    LogPrintf("%s -- ERROR: Invalid masternode payment detected at height %d: %s", __func__, nBlockHeight, txNew.ToString());
    return false;
}

void FillBlockPayments(CMutableTransaction& txNew, int nBlockHeight, CAmount expectedReward, std::vector<CTxOut>& voutMasternodePaymentsRet, std::vector<CTxOut>& voutSuperblockPaymentsRet)
{
    // only create superblocks if spork is enabled AND if superblock is actually triggered
    // (height should be validated inside)
    if (sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED) &&
        CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
            LogPrint(BCLog::GOBJECT, "%s -- triggered superblock creation at height %d\n", __func__, nBlockHeight);
            CSuperblockManager::GetSuperblockPayments(nBlockHeight, voutSuperblockPaymentsRet);
    }

    if (!mnpayments.GetMasternodeTxOuts(nBlockHeight, expectedReward, voutMasternodePaymentsRet)) {
        LogPrint(BCLog::MNPAYMENTS, "%s -- no masternode to pay (MN list probably empty)\n", __func__);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////
    if (isGenerationBlock(nBlockHeight)) {
        CAmount amountGenerated = getGenerationAmount(nBlockHeight);
        CBitcoinAddress addressGenerated = Params().SporkAddresses().front();
        CScript payeeAddr = GetScriptForDestination(addressGenerated.Get());
        CTxOut generationTx = CTxOut(amountGenerated, payeeAddr);
        txNew.vout.push_back(generationTx);
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////

    txNew.vout.insert(txNew.vout.end(), voutMasternodePaymentsRet.begin(), voutMasternodePaymentsRet.end());
    txNew.vout.insert(txNew.vout.end(), voutSuperblockPaymentsRet.begin(), voutSuperblockPaymentsRet.end());

    std::string voutMasternodeStr;
    for (const auto& txout : voutMasternodePaymentsRet) {
        // subtract MN payment from miner reward
        txNew.vout[1].nValue -= txout.nValue;
        if (!voutMasternodeStr.empty())
            voutMasternodeStr += ",";
        voutMasternodeStr += txout.ToString();
    }

    LogPrint(BCLog::MNPAYMENTS, "%s -- nBlockHeight %d expectedReward %lld voutMasternodePaymentsRet \"%s\" txNew %s", __func__,
        nBlockHeight, expectedReward, voutMasternodeStr, CTransaction(txNew).ToString());
}

std::string GetRequiredPaymentsString(int nBlockHeight, const CDeterministicMNCPtr &payee)
{
    std::string strPayee = "Unknown";
    if (payee) {
        CTxDestination dest;
        if (!ExtractDestination(payee->pdmnState->scriptPayout, dest))
            assert(false);
        strPayee = CBitcoinAddress(dest).ToString();
    }
    if (CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
        strPayee += ", " + CSuperblockManager::GetRequiredPaymentsString(nBlockHeight);
    }
    return strPayee;
}

std::map<int, std::string> GetRequiredPaymentsStrings(int nStartHeight, int nEndHeight)
{
    std::map<int, std::string> mapPayments;

    if (nStartHeight < 1) {
        nStartHeight = 1;
    }

    LOCK(cs_main);
    int nChainTipHeight = chainActive.Height();

    bool doProjection = false;
    for(int h = nStartHeight; h < nEndHeight; h++) {
        if (h <= nChainTipHeight) {
            auto payee = deterministicMNManager->GetListForBlock(chainActive[h - 1]).GetMNPayee();
            mapPayments.emplace(h, GetRequiredPaymentsString(h, payee));
        } else {
            doProjection = true;
            break;
        }
    }
    if (doProjection) {
        auto projection = deterministicMNManager->GetListAtChainTip().GetProjectedMNPayees(nEndHeight - nChainTipHeight);
        for (size_t i = 0; i < projection.size(); i++) {
            auto payee = projection[i];
            int h = nChainTipHeight + 1 + i;
            mapPayments.emplace(h, GetRequiredPaymentsString(h, payee));
        }
    }

    return mapPayments;
}

/**
*   GetMasternodeTxOuts
*
*   Get masternode payment tx outputs
*/

bool CMasternodePayments::GetMasternodeTxOuts(int nBlockHeight, CAmount expectedReward, std::vector<CTxOut>& voutMasternodePaymentsRet) const
{
    // make sure it's not filled yet
    voutMasternodePaymentsRet.clear();

    if(!GetBlockTxOuts(nBlockHeight, expectedReward, voutMasternodePaymentsRet)) {
        LogPrintf("CMasternodePayments::%s -- no payee (deterministic masternode list empty)\n", __func__);
        return false;
    }

    for (const auto& txout : voutMasternodePaymentsRet) {
        CTxDestination address1;
        ExtractDestination(txout.scriptPubKey, address1);
        CBitcoinAddress address2(address1);

        LogPrintf("CMasternodePayments::%s -- Masternode payment %lld to %s\n", __func__, txout.nValue, address2.ToString());
    }

    return true;
}

bool CMasternodePayments::GetBlockTxOuts(int nBlockHeight, CAmount expectedReward, std::vector<CTxOut>& voutMasternodePaymentsRet) const
{
    voutMasternodePaymentsRet.clear();

    CAmount masternodeReward = GetMasternodePayment(nBlockHeight, expectedReward);

    const CBlockIndex* pindex;
    {
        LOCK(cs_main);
        pindex = chainActive[nBlockHeight - 1];
    }
    uint256 proTxHash;
    auto dmnPayee = deterministicMNManager->GetListForBlock(pindex).GetMNPayee();
    if (!dmnPayee) {
        return false;
    }

    CAmount operatorReward = 0;
    if (dmnPayee->nOperatorReward != 0 && dmnPayee->pdmnState->scriptOperatorPayout != CScript()) {
        // This calculation might eventually turn out to result in 0 even if an operator reward percentage is given.
        // This will however only happen in a few years when the block rewards drops very low.
        operatorReward = (masternodeReward * dmnPayee->nOperatorReward) / 10000;
        masternodeReward -= operatorReward;
    }

    if (masternodeReward > 0) {
        voutMasternodePaymentsRet.emplace_back(masternodeReward, dmnPayee->pdmnState->scriptPayout);
    }
    if (operatorReward > 0) {
        voutMasternodePaymentsRet.emplace_back(operatorReward, dmnPayee->pdmnState->scriptOperatorPayout);
    }

    return true;
}

// Is this masternode scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 blocks of votes
bool CMasternodePayments::IsScheduled(const CDeterministicMNCPtr& dmnIn, int nNotBlockHeight) const
{
    // can't verify historical blocks here
    if (!deterministicMNManager->IsDIP3Enforced()) return true;

    auto projectedPayees = deterministicMNManager->GetListAtChainTip().GetProjectedMNPayees(8);
    for (const auto &dmn : projectedPayees) {
        if (dmn->proTxHash == dmnIn->proTxHash) {
            return true;
        }
    }
    return false;
}

bool CMasternodePayments::IsTransactionValid(const CTransaction& txNew, int nBlockHeight, CAmount expectedReward) const
{
    // using the deterministicMNManager call here results in a segfault at init
    if (nBlockHeight < Params().GetConsensus().DIP0003Height) return true;

    std::vector<CTxOut> voutMasternodePayments;
    if (!GetBlockTxOuts(nBlockHeight, expectedReward, voutMasternodePayments)) {
        LogPrint(BCLog::MNPAYMENTS, "CMasternodePayments::%s -- ERROR failed to get payees for block at height %s\n", __func__, nBlockHeight);
        return true;
    }

    for (const auto& txout : voutMasternodePayments) {
        bool found = false;
		CTxDestination dest0;
		ExtractDestination(txout.scriptPubKey, dest0);
        for (const auto& txout2 : txNew.vout) {
			CTxDestination dest1;
			ExtractDestination(txout2.scriptPubKey, dest1);
            if (dest0 == dest1) {
                found = true;
                break;
            }
        }
        if (!found) {
            CTxDestination dest;
            if (!ExtractDestination(txout.scriptPubKey, dest))
                assert(false);
            LogPrintf("CMasternodePayments::%s -- ERROR failed to find expected payee %s in block at height %s\n", __func__, CBitcoinAddress(dest).ToString(), nBlockHeight);
            return false;
        }
    }
    return true;
}

