// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2014-2020 The Dash Core developers
// Copyright (c) 2018-2020 The PACGlobal developers
// Copyright (c) 2021 The Lokal Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include "arith_uint256.h"

#include <assert.h>

#include "chainparamsseeds.h"

const int32_t NEVER32 = 400000U;
const int64_t NEVER64 = 4070908800ULL;

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
	CMutableTransaction txNew;
	txNew.nVersion = 1;
	txNew.vin.resize(1);
	txNew.vout.resize(1);
	txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
	txNew.vout[0].nValue = genesisReward;
	txNew.vout[0].scriptPubKey = genesisOutputScript;

	CBlock genesis;
	genesis.nTime = nTime;
	genesis.nBits = nBits;
	genesis.nNonce = nNonce;
	genesis.nVersion = nVersion;
	genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
	genesis.hashPrevBlock.SetNull();
	genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
	return genesis;
}

static CBlock CreateDevNetGenesisBlock(const uint256 &prevBlockHash, const std::string& devNetName, uint32_t nTime, uint32_t nNonce, uint32_t nBits, const CAmount& genesisReward)
{
	assert(!devNetName.empty());

	CMutableTransaction txNew;
	txNew.nVersion = 1;
	txNew.vin.resize(1);
	txNew.vout.resize(1);
	// put height (BIP34) and devnet name into coinbase
	txNew.vin[0].scriptSig = CScript() << 1 << std::vector<unsigned char>(devNetName.begin(), devNetName.end());
	txNew.vout[0].nValue = genesisReward;
	txNew.vout[0].scriptPubKey = CScript() << OP_RETURN;

	CBlock genesis;
	genesis.nTime = nTime;
	genesis.nBits = nBits;
	genesis.nNonce = nNonce;
	genesis.nVersion = 4;
	genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
	genesis.hashPrevBlock = prevBlockHash;
	genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
	return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=00000ffd590b14, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=e0028e, nTime=1390095618, nBits=1e0ffff0, nNonce=28917698, vtx=1)
 *   CTransaction(hash=e0028e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d01044c5957697265642030392f4a616e2f3230313420546865204772616e64204578706572696d656e7420476f6573204c6976653a204f76657273746f636b2e636f6d204973204e6f7720416363657074696e6720426974636f696e73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0xA9037BAC7050C479B121CF)
 *   vMerkleTree: e0028e
 */

static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward, bool fTestnet)
{
	const char* pszTimestamp = "Bitcoin performed 10 times better than gold in 2020";
	const CScript genesisOutputScript = CScript() << ParseHex("0411345e927d2d3abb85541e23b211f5a9019f2b240fb6bd4b1c44234993639793846cfc74154d293a3bf7ba74592f5f358127c0062a621d3b153089d0f5bb84e5") << OP_CHECKSIG;
	return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}


void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout, int64_t nWindowSize, int64_t nThreshold)
{
	consensus.vDeployments[d].nStartTime = nStartTime;
	consensus.vDeployments[d].nTimeout = nTimeout;
	if (nWindowSize != -1) {
		consensus.vDeployments[d].nWindowSize = nWindowSize;
	}
	if (nThreshold != -1) {
		consensus.vDeployments[d].nThreshold = nThreshold;
	}
}

void CChainParams::UpdateDIP3Parameters(int nActivationHeight, int nEnforcementHeight)
{
	consensus.DIP0003Height = nActivationHeight;
	consensus.DIP0003EnforcementHeight = nEnforcementHeight;
}

void CChainParams::UpdateBudgetParameters(int nMasternodePaymentsStartBlock, int nBudgetPaymentsStartBlock, int nSuperblockStartBlock)
{
	consensus.nMasternodePaymentsStartBlock = nMasternodePaymentsStartBlock;
	consensus.nBudgetPaymentsStartBlock = nBudgetPaymentsStartBlock;
	consensus.nSuperblockStartBlock = nSuperblockStartBlock;
}

void CChainParams::UpdateSubsidyAndDiffParams(int nMinimumDifficultyBlocks, int nHighSubsidyBlocks, int nHighSubsidyFactor)
{
	consensus.nMinimumDifficultyBlocks = nMinimumDifficultyBlocks;
	consensus.nHighSubsidyBlocks = nHighSubsidyBlocks;
	consensus.nHighSubsidyFactor = nHighSubsidyFactor;
}

void CChainParams::UpdateLLMQChainLocks(Consensus::LLMQType llmqType) {
	consensus.llmqTypeChainLocks = llmqType;
}

static CBlock FindDevNetGenesisBlock(const Consensus::Params& params, const CBlock &prevBlock, const CAmount& reward)
{
	std::string devNetName = GetDevNetName();
	assert(!devNetName.empty());

	CBlock block = CreateDevNetGenesisBlock(prevBlock.GetHash(), devNetName.c_str(), prevBlock.nTime + 1, 0, prevBlock.nBits, reward);

	arith_uint256 bnTarget;
	bnTarget.SetCompact(block.nBits);

	for (uint32_t nNonce = 0; nNonce < UINT32_MAX; nNonce++) {
		block.nNonce = nNonce;

		uint256 hash = block.GetHash();
		if (UintToArith256(hash) <= bnTarget)
			return block;
	}

	// This is very unlikely to happen as we start the devnet with a very low difficulty. In many cases even the first
	// iteration of the above loop will give a result already
	error("FindDevNetGenesisBlock: could not find devnet genesis block for %s", devNetName);
	assert(false);
}

// this one is for testing only
static Consensus::LLMQParams llmq5_60 = {
		.type = Consensus::LLMQ_5_60,
		.name = "llmq_5_60",
		.size = 3,
		.minSize = 3,
		.threshold = 3,

		.dkgInterval = 24, // one DKG per hour
		.dkgPhaseBlocks = 2,
		.dkgMiningWindowStart = 10, // dkgPhaseBlocks * 5 = after finalization
		.dkgMiningWindowEnd = 18,
		.dkgBadVotesThreshold = 8,

		.signingActiveQuorumCount = 2, // just a few ones to allow easier testing

		.keepOldConnections = 3,
};

static Consensus::LLMQParams llmq50_60 = {
		.type = Consensus::LLMQ_50_60,
		.name = "llmq_50_60",
		.size = 50,
		.minSize = 20,
		.threshold = 10,

		.dkgInterval = 60, // one DKG per hour
		.dkgPhaseBlocks = 5,
		.dkgMiningWindowStart = 25, // dkgPhaseBlocks * 5 = after finalization
		.dkgMiningWindowEnd = 45,
		.dkgBadVotesThreshold = 40,

		.signingActiveQuorumCount = 24, // a full day worth of LLMQs

		.keepOldConnections = 25,
};

static Consensus::LLMQParams llmq400_60 = {
		.type = Consensus::LLMQ_400_60,
		.name = "llmq_400_60",
		.size = 400,
		.minSize = 100,
		.threshold = 70,

		.dkgInterval = 60 * 12, // one DKG every 12 hours
		.dkgPhaseBlocks = 10,
		.dkgMiningWindowStart = 50, // dkgPhaseBlocks * 5 = after finalization
		.dkgMiningWindowEnd = 70,
		.dkgBadVotesThreshold = 150,

		.signingActiveQuorumCount = 4, // two days worth of LLMQs

		.keepOldConnections = 5,
};

// Used for deployment and min-proto-version signalling, so it needs a higher threshold
static Consensus::LLMQParams llmq400_85 = {
		.type = Consensus::LLMQ_400_85,
		.name = "llmq_400_85",
		.size = 400,
		.minSize = 150,
		.threshold = 100,

		.dkgInterval = 60 * 24, // one DKG every 24 hours
		.dkgPhaseBlocks = 10,
		.dkgMiningWindowStart = 50, // dkgPhaseBlocks * 5 = after finalization
		.dkgMiningWindowEnd = 120, // give it a larger mining window to make sure it is mined
		.dkgBadVotesThreshold = 300,

		.signingActiveQuorumCount = 4, // four days worth of LLMQs

		.keepOldConnections = 5,
};

/**
 * Main network
 */
 /**
  * What makes a good checkpoint block?
  * + Is surrounded by blocks with reasonable timestamps
  *   (no blocks before with a timestamp after, none after with
  *    timestamp before)
  * + Contains no strange transactions
  */


class CMainParams : public CChainParams {
public:
	CMainParams() {
		strNetworkID = "main";
		consensus.nMasternodePaymentsStartBlock = 201;
		consensus.nInstantSendConfirmationsRequired = 6;
		consensus.nInstantSendKeepLock = 24;
		consensus.nBudgetPaymentsStartBlock = NEVER32; // actual historical value
		consensus.nBudgetPaymentsCycleBlocks = NEVER32; // ~(60*24*30)/2.6, actual number of blocks per month is 200700 / 12 = 16725
		consensus.nBudgetPaymentsWindowBlocks = NEVER32;
		consensus.nSuperblockStartBlock = NEVER32;
		consensus.nSuperblockStartHash = uint256S("");
		consensus.nSuperblockCycle = NEVER32;
		consensus.nGovernanceMinQuorum = 10;
		consensus.nGovernanceFilterElements = 20000;
		consensus.nHardenedStakeCheckHeight = 387939;
		consensus.nMasternodeMinimumConfirmations = 15;
		consensus.nMasternodeCollateral = 20000 * COIN;
		consensus.BIP34Height = 1;
		consensus.BIP34Hash = uint256S("0x");
		consensus.BIP65Height = 1;
		consensus.BIP66Height = 400;
		consensus.DIP0001Height = 2;
		consensus.DIP0003Height = 201;
		consensus.DIP0003EnforcementHeight = 2000;
		consensus.DIP0003EnforcementHash = uint256();
		consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
		consensus.posLimit = uint256S("07ffff0000000000000000000000000000000000000000000000000000000000");
		consensus.nLastPoWBlock = 200;
		consensus.nPowTargetTimespan = 24 * 60 * 60;
		consensus.nPowTargetSpacing = 2 * 60;
		consensus.nPosTargetSpacing = consensus.nPowTargetSpacing;
		consensus.nPosTargetTimespan = consensus.nPowTargetTimespan;
		consensus.nMinimumStakeValue = 3 * COIN;
		consensus.nStakeMinAge = 60 * 60;
		consensus.nStakeMaxAge = 60 * 60 * 24 * 365;
		consensus.nModifierInterval = 60 * 20;
		consensus.fPowAllowMinDifficultyBlocks = false;
		consensus.fPowNoRetargeting = false;
		consensus.nPowKGWHeight = 20;
		consensus.nPowDGWHeight = 60;
		consensus.nRuleChangeActivationThreshold = 1916;
		consensus.nMinerConfirmationWindow = 2016;

		consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
		consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1573325000;
		consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = NEVER64;

		// Deployment of BIP68, BIP112, and BIP113.
		consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
		consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1573325000;
		consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = NEVER64;

		// Deployment of DIP0001
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].bit = 1;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nStartTime = 1573325000;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nTimeout = NEVER64;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nWindowSize = 100;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nThreshold = 50;

		// Deployment of BIP147
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].bit = 2;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nStartTime = 1573325000;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nTimeout = NEVER64;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nWindowSize = 100;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nThreshold = 50;

		// Deployment of DIP0003
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].bit = 3;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nWindowSize = 2000;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nThreshold = 1000;

		// Deployment of DIP0008
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].bit = 4;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nWindowSize = 3000;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nThreshold = 1500;

		// The best chain should have at least this much work.
		consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");  // 332500

		// By default assume that the signatures in ancestors of this block are valid.
		consensus.defaultAssumeValid = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");

		/**
		 * The message start string is designed to be unlikely to occur in normal data.
		 * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
		 * a large 32-bit integer with any alignment.
		 */
		pchMessageStart[0] = 0xac;
		pchMessageStart[1] = 0xe5;
		pchMessageStart[2] = 0xb6;
		pchMessageStart[3] = 0x7c;
		nDefaultPort = 2513;
		nPruneAfterHeight = 100000;

		//uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward, bool fTestnet
		genesis = CreateGenesisBlock(1613019600, 2024315, 0x1e0ffff0, 1, 0 * COIN, false);
		consensus.hashGenesisBlock = genesis.GetHash();
		assert(consensus.hashGenesisBlock == uint256S("0x00000b49f79eaf2a0a99f3d85762a63b410711953933834c9afd5e96ce805a61"));
		assert(genesis.hashMerkleRoot == uint256S("0xbafd9ea271b5fdc00fefe68e82c3baaa5b2b9f0770c93713506fb071fe4337fa"));

		vSeeds.emplace_back("66.42.72.163");
		vSeeds.emplace_back("2001:19f0:8001:1a67:5400:03ff:fe09:0022");
		vSeeds.emplace_back("66.42.61.57");
		vSeeds.emplace_back("2001:19f0:4400:7a35:5400:03ff:fe08:ffac");
		vSeeds.emplace_back("108.61.188.47");
		vSeeds.emplace_back("2001:19f0:5001:11a2:5400:03ff:fe08:ffb9");
		vSeeds.emplace_back("108.61.209.126");
		vSeeds.emplace_back("2a05:f480:1c00:a59:5400:03ff:fe08:ffcc");
		vSeeds.emplace_back("192.248.187.37");
		vSeeds.emplace_back("2001:19f0:6c01:2e7f:5400:03ff:fe08:ffe6");
		vSeeds.emplace_back("216.128.128.44");
		vSeeds.emplace_back("2001:19f0:6401:1f0e:5400:03ff:fe08:fffe");
		vSeeds.emplace_back("155.138.131.22");
		vSeeds.emplace_back("2001:19f0:b001:7e6:5400:03ff:fe09:0011");
		vSeeds.emplace_back("45.76.120.11");
		vSeeds.emplace_back("2401:c080:1800:410e:5400:03ff:fe09:8fc5");

		// LOKAL addresses start with 'L'
		base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 48);
		// LOKAL script addresses start with '5'
		base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 10);
		// LOKAL private keys start with '7' or 'X'
		base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 204);
		// LOKAL BIP32 pubkeys start with 'xpub' (Bitcoin defaults)
		base58Prefixes[EXT_PUBLIC_KEY] = { 0x04, 0x88, 0xB2, 0x1E };
		// LOKAL BIP32 prvkeys start with 'xprv' (Bitcoin defaults)
		base58Prefixes[EXT_SECRET_KEY] = { 0x04, 0x88, 0xAD, 0xE4 };

		// LOKAL BIP44 coin type is '5'
		nExtCoinType = 5;

		vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

		// long living quorum params
		consensus.llmqs[Consensus::LLMQ_50_60] = llmq50_60;
		consensus.llmqs[Consensus::LLMQ_400_60] = llmq400_60;
		consensus.llmqs[Consensus::LLMQ_400_85] = llmq400_85;
		consensus.llmqTypeChainLocks = Consensus::LLMQ_400_60;
		consensus.llmqForInstaLOKAL = Consensus::LLMQ_50_60;

		fDefaultConsistencyChecks = false;
		fRequireStandard = true;
		fRequireRoutableExternalIP = true;
		fMineBlocksOnDemand = false;
		fAllowMultipleAddressesFromGroup = false;
		fAllowMultiplePorts = false;

		nPoolMinParticipants = 3;
		nPoolMaxParticipants = 5;
		nFulfilledRequestExpireTime = 60 * 60; // fulfilled requests expire in 1 hour

		vSporkAddresses = { "LYUfeNSmdy5dehc8MJq2zmySt16bWjSNk3" };
		nMinSporkKeys = 1;
		fBIP9CheckMasternodesUpgraded = true;

		checkpointData = (CCheckpointData) {
			{
				{  0, uint256S("0x00000b49f79eaf2a0a99f3d85762a63b410711953933834c9afd5e96ce805a61")},
				{ 87, uint256S("0x0000002832516c11599844772ae91e71d6aef4d9f20fdd405a930e58aeeb55bc") },
				{ 206, uint256S("0xb6c81cf54e68ba3f9e9afc6386ede02d9fa400b3a12394dcf3081561f9e3e16f") },
				{ 1523, uint256S("0xc2da096484f77b259a15883b908ea638d98ad2da2a9c6563f50fa0b5fb042410") },
				{ 5049, uint256S("0x545ac4052680e138a24e9c1fe1e46e0a66a8aa80ddedaded07f9b9df15431a9f") },
				{ 7321, uint256S("0x17e6e184ef23cc8637bebd0d34f0ec6701a99c186cc4c6e8741e64cfeb3ee5d8") },
				{ 9301, uint256S("0x939e18fa7935bf86e46d1d34a191c7ee85c4ac2ea33135d9ac7bd21f6df6eeb9") },
			}
		};

		chainTxData = ChainTxData{
			1614229693, // * UNIX timestamp of last known number of transactions
			20171,      // * total number of transactions between genesis and that timestamp
						//   (the tx=... number in the SetBestChain debug.log lines)
			0.01        // * estimated number of transactions per second after that timestamp
		};
	}
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
	CTestNetParams() {
		strNetworkID = "test";
		consensus.nMasternodePaymentsStartBlock = 50;
		consensus.nInstantSendConfirmationsRequired = 2;
		consensus.nInstantSendKeepLock = 6;
		consensus.nBudgetPaymentsStartBlock = 50;
		consensus.nBudgetPaymentsCycleBlocks = 50;
		consensus.nBudgetPaymentsWindowBlocks = 100;
		consensus.nSuperblockStartBlock = 100;
		consensus.nSuperblockStartHash = uint256();
		consensus.nSuperblockCycle = 24;
		consensus.nGovernanceMinQuorum = 1;
		consensus.nGovernanceFilterElements = 500;
		consensus.nMasternodeMinimumConfirmations = 15;
		consensus.nMasternodeCollateral = 1000 * COIN;
		consensus.BIP34Height = 1;
		consensus.BIP34Hash = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");
		consensus.BIP65Height = 0;
		consensus.BIP66Height = 0;
		consensus.DIP0001Height = 1;
		consensus.DIP0003Height = 75;
		consensus.DIP0003EnforcementHeight = 363000;
		consensus.DIP0003EnforcementHash = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");
		consensus.powLimit = uint256S("0000fffff0000000000000000000000000000000000000000000000000000000");
		consensus.posLimit = uint256S("007ffff000000000000000000000000000000000000000000000000000000000");
		consensus.nLastPoWBlock = consensus.DIP0003Height;
		consensus.nPowTargetTimespan = 60;
		consensus.nPowTargetSpacing = 60;
		consensus.nPosTargetSpacing = consensus.nPowTargetSpacing;
		consensus.nPosTargetTimespan = consensus.nPowTargetTimespan;
		consensus.nMinimumStakeValue = 100 * COIN;
		consensus.nStakeMinAge = 10 * 60;
		consensus.nStakeMaxAge = 60 * 60 * 24 * 30;
		consensus.nModifierInterval = 60 * 20;
		consensus.fPowAllowMinDifficultyBlocks = false;
		consensus.fPowNoRetargeting = false;
		consensus.nPowKGWHeight = NEVER32; // unused
		consensus.nPowDGWHeight = NEVER32; // unused
		consensus.nRuleChangeActivationThreshold = 1512;
		consensus.nMinerConfirmationWindow = 2016;

		consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
		consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1573325000;
		consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = NEVER64;

		// Deployment of BIP68, BIP112, and BIP113.
		consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
		consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1573325000;
		consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = NEVER64;

		// Deployment of DIP0001
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].bit = 1;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nStartTime = 1573325000;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nTimeout = NEVER64;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nWindowSize = 100;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nThreshold = 50;

		// Deployment of BIP147
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].bit = 2;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nStartTime = 1573325000;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nTimeout = NEVER64;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nWindowSize = 100;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nThreshold = 50;

		// Deployment of DIP0003
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].bit = 3;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nWindowSize = 1000;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nThreshold = 250;

		// Deployment of DIP0008
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].bit = 4;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nWindowSize = 1000;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nThreshold = 250;

		// The best chain should have at least this much work.
		consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");

		// By default assume that the signatures in ancestors of this block are valid.
		consensus.defaultAssumeValid = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");

		pchMessageStart[0] = 0x22;
		pchMessageStart[1] = 0x44;
		pchMessageStart[2] = 0x66;
		pchMessageStart[3] = 0x88;
		nDefaultPort = 29999;
		nPruneAfterHeight = 1000;

		genesis = CreateGenesisBlock(1573325000, 11404, 0x1f00ffff, 1, 0 * COIN, true);
		consensus.hashGenesisBlock = genesis.GetHash();

		vSeeds.clear();
		vFixedSeeds.clear();

		// Testnet LOKAL_Coin addresses start with 'y'
		base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 140);
		// Testnet LOKAL_Coin script addresses start with '8' or '9'
		base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 19);
		// Testnet private keys start with '9' or 'c' (Bitcoin defaults)
		base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);
		// Testnet LOKAL_Coin BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
		base58Prefixes[EXT_PUBLIC_KEY] = { 0x04, 0x35, 0x87, 0xCF };
		// Testnet LOKAL_Coin BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
		base58Prefixes[EXT_SECRET_KEY] = { 0x04, 0x35, 0x83, 0x94 };

		// Testnet LOKAL_Coin BIP44 coin type is '1' (All coin's testnet default)
		nExtCoinType = 1;

		// long living quorum params
		consensus.llmqs[Consensus::LLMQ_50_60] = llmq50_60;
		consensus.llmqs[Consensus::LLMQ_400_60] = llmq400_60;
		consensus.llmqs[Consensus::LLMQ_400_85] = llmq400_85;
		consensus.llmqTypeChainLocks = Consensus::LLMQ_50_60;
		consensus.llmqForInstaLOKAL = Consensus::LLMQ_50_60;

		fDefaultConsistencyChecks = false;
		fRequireStandard = true;
		fRequireRoutableExternalIP = false;
		fMineBlocksOnDemand = false;
		fAllowMultipleAddressesFromGroup = true;
		fAllowMultiplePorts = true;

		nPoolMinParticipants = 3;
		nPoolMaxParticipants = 5;
		nFulfilledRequestExpireTime = 5 * 60; // fulfilled requests expire in 5 minutes

		vSporkAddresses = { "yTpFjxs3Rtwe7MXfC1i5XACz2K5UYi2GpL" };
		nMinSporkKeys = 1;
		fBIP9CheckMasternodesUpgraded = true;

		checkpointData = (CCheckpointData) {};

		chainTxData = ChainTxData{
			1567342000, // * UNIX timestamp of last known number of transactions
			1,          // * total number of transactions between genesis and that timestamp
						//   (the tx=... number in the SetBestChain debug.log lines)
			1.0         // * estimated number of transactions per second after that timestamp
		};

	}
};

/**
 * Devnet
 */
class CDevNetParams : public CChainParams {
public:
	CDevNetParams() {
		strNetworkID = "dev";
		consensus.nMasternodePaymentsStartBlock = 4010;
		consensus.nInstantSendConfirmationsRequired = 2;
		consensus.nInstantSendKeepLock = 6;
		consensus.nBudgetPaymentsStartBlock = 4100;
		consensus.nBudgetPaymentsCycleBlocks = 50;
		consensus.nBudgetPaymentsWindowBlocks = 10;
		consensus.nSuperblockStartBlock = 4200; // NOTE: Should satisfy nSuperblockStartBlock > nBudgetPeymentsStartBlock
		consensus.nSuperblockStartHash = uint256(); // do not check this on devnet
		consensus.nSuperblockCycle = 24; // Superblocks can be issued hourly on devnet
		consensus.nGovernanceMinQuorum = 1;
		consensus.nGovernanceFilterElements = 500;
		consensus.nMasternodeMinimumConfirmations = 1;
		consensus.nMasternodeCollateral = 500000 * COIN;
		consensus.BIP34Height = 1; // BIP34 activated immediately on devnet
		consensus.BIP65Height = 1; // BIP65 activated immediately on devnet
		consensus.BIP66Height = 1; // BIP66 activated immediately on devnet
		consensus.DIP0001Height = 2; // DIP0001 activated immediately on devnet
		consensus.DIP0003Height = 2; // DIP0003 activated immediately on devnet
		consensus.DIP0003EnforcementHeight = 2; // DIP0003 activated immediately on devnet
		consensus.DIP0003EnforcementHash = uint256();
		consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
		consensus.posLimit = uint256S("007ffff000000000000000000000000000000000000000000000000000000000");
		consensus.nLastPoWBlock = 100;
		consensus.nPowTargetTimespan = 24 * 60 * 60; // LOKAL_Coin: 1 day
		consensus.nPowTargetSpacing = 2.5 * 60; // LOKAL_Coin: 2.5 minutes
		consensus.nPosTargetSpacing = consensus.nPowTargetSpacing;
		consensus.nPosTargetTimespan = consensus.nPowTargetTimespan;
		consensus.nMinimumStakeValue = 10000 * COIN;
		consensus.nStakeMinAge = 10 * 60;
		consensus.nStakeMaxAge = 60 * 60 * 24 * 30;
		consensus.nModifierInterval = 60 * 20;
		consensus.fPowAllowMinDifficultyBlocks = true;
		consensus.fPowNoRetargeting = false;
		consensus.nPowKGWHeight = 4001; // nPowKGWHeight >= nPowDGWHeight means "no KGW"
		consensus.nPowDGWHeight = 4001;
		consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
		consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
		consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
		consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
		consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

		// Deployment of BIP68, BIP112, and BIP113.
		consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
		consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1506556800; // September 28th, 2017
		consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1538092800; // September 28th, 2018

		// Deployment of DIP0001
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].bit = 1;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nStartTime = 1505692800; // Sep 18th, 2017
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nTimeout = 1537228800; // Sep 18th, 2018
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nWindowSize = 100;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nThreshold = 50; // 50% of 100

		// Deployment of BIP147
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].bit = 2;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nStartTime = 1517792400; // Feb 5th, 2018
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nTimeout = 1549328400; // Feb 5th, 2019
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nWindowSize = 100;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nThreshold = 50; // 50% of 100

		// Deployment of DIP0003
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].bit = 3;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nStartTime = 1535752800; // Sep 1st, 2018
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nTimeout = 1567288800; // Sep 1st, 2019
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nWindowSize = 100;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nThreshold = 50; // 50% of 100

		// Deployment of DIP0008
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].bit = 4;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nStartTime = 1553126400; // Mar 21st, 2019
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nTimeout = 1584748800; // Mar 21st, 2020
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nWindowSize = 100;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nThreshold = 50; // 50% of 100

		// The best chain should have at least this much work.
		consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000000000000000");

		// By default assume that the signatures in ancestors of this block are valid.
		consensus.defaultAssumeValid = uint256S("0x000000000000000000000000000000000000000000000000000000000000000");

		pchMessageStart[0] = 0xe2;
		pchMessageStart[1] = 0xca;
		pchMessageStart[2] = 0xff;
		pchMessageStart[3] = 0xce;
		nDefaultPort = 19799;
		nPruneAfterHeight = 1000;

		genesis = CreateGenesisBlock(1417713337, 1096447, 0x207fffff, 1, 50 * COIN, false);
		consensus.hashGenesisBlock = genesis.GetHash();
		assert(consensus.hashGenesisBlock == uint256S("0x000008ca1832a4baf228eb1553c03d3a2c8e02399550dd6ea8d65cec3ef23d2e"));
		assert(genesis.hashMerkleRoot == uint256S("0xe0028eb9648db56b1ac77cf090b99048a8007e2bb64b68f092c03c7f56a662c7"));

		devnetGenesis = FindDevNetGenesisBlock(consensus, genesis, 50 * COIN);
		consensus.hashDevnetGenesisBlock = devnetGenesis.GetHash();

		vFixedSeeds.clear();
		vSeeds.clear();
		//vSeeds.push_back(CDNSSeedData("lokalevo.org",  "devnet-seed.lokalevo.org"));

		// Testnet LOKAL_Coin addresses start with 'y'
		base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 140);
		// Testnet LOKAL_Coin script addresses start with '8' or '9'
		base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 19);
		// Testnet private keys start with '9' or 'c' (Bitcoin defaults)
		base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);
		// Testnet LOKAL_Coin BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
		base58Prefixes[EXT_PUBLIC_KEY] = { 0x04, 0x35, 0x87, 0xCF };
		// Testnet LOKAL_Coin BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
		base58Prefixes[EXT_SECRET_KEY] = { 0x04, 0x35, 0x83, 0x94 };

		// Testnet LOKAL_Coin BIP44 coin type is '1' (All coin's testnet default)
		nExtCoinType = 1;

		// long living quorum params
		consensus.llmqs[Consensus::LLMQ_50_60] = llmq50_60;
		consensus.llmqs[Consensus::LLMQ_400_60] = llmq400_60;
		consensus.llmqs[Consensus::LLMQ_400_85] = llmq400_85;
		consensus.llmqTypeChainLocks = Consensus::LLMQ_50_60;
		consensus.llmqForInstaLOKAL = Consensus::LLMQ_50_60;

		fDefaultConsistencyChecks = false;
		fRequireStandard = false;
		fMineBlocksOnDemand = false;
		fAllowMultipleAddressesFromGroup = true;
		fAllowMultiplePorts = true;

		nPoolMinParticipants = 3;
		nPoolMaxParticipants = 5;
		nFulfilledRequestExpireTime = 5 * 60; // fulfilled requests expire in 5 minutes

		vSporkAddresses = { "yjPtiKh2uwk3bDutTEA2q9mCtXyiZRWn55" };
		nMinSporkKeys = 1;
		// devnets are started with no blocks and no MN, so we can't check for upgraded MN (as there are none)
		fBIP9CheckMasternodesUpgraded = false;

		checkpointData = (CCheckpointData) {
			{
				{ 0, uint256S("0x000008ca1832a4baf228eb1553c03d3a2c8e02399550dd6ea8d65cec3ef23d2e")},
				{ 1, devnetGenesis.GetHash() },
			}
		};

		chainTxData = ChainTxData{
			devnetGenesis.GetBlockTime(), // * UNIX timestamp of devnet genesis block
			2,                            // * we only have 2 coinbase transactions when a devnet is started up
			0.01                          // * estimated number of transactions per second
		};
	}
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
	CRegTestParams() {
		strNetworkID = "regtest";
		consensus.nMasternodePaymentsStartBlock = 240;
		consensus.nInstantSendConfirmationsRequired = 2;
		consensus.nInstantSendKeepLock = 6;
		consensus.nBudgetPaymentsStartBlock = 1000;
		consensus.nBudgetPaymentsCycleBlocks = 50;
		consensus.nBudgetPaymentsWindowBlocks = 10;
		consensus.nSuperblockStartBlock = 1500;
		consensus.nSuperblockStartHash = uint256(); // do not check this on regtest
		consensus.nSuperblockCycle = 10;
		consensus.nGovernanceMinQuorum = 1;
		consensus.nGovernanceFilterElements = 100;
		consensus.nMasternodeMinimumConfirmations = 1;
		consensus.nMasternodeCollateral = 500000 * COIN;
		consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
		consensus.BIP34Hash = uint256();
		consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
		consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
		consensus.DIP0001Height = 2000;
		consensus.DIP0003Height = 432;
		consensus.DIP0003EnforcementHeight = 500;
		consensus.DIP0003EnforcementHash = uint256();
		consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
		consensus.posLimit = uint256S("007ffff000000000000000000000000000000000000000000000000000000000");
		consensus.nLastPoWBlock = 100;
		consensus.nPowTargetTimespan = 24 * 60 * 60; // LOKAL_Coin: 1 day
		consensus.nPowTargetSpacing = 2.5 * 60; // LOKAL_Coin: 2.5 minutes
		consensus.nPosTargetSpacing = consensus.nPowTargetSpacing;
		consensus.nPosTargetTimespan = consensus.nPowTargetTimespan;
		consensus.nStakeMinAge = 10 * 60;
		consensus.nStakeMaxAge = 60 * 60 * 24 * 30;
		consensus.nModifierInterval = 60 * 20;
		consensus.fPowAllowMinDifficultyBlocks = true;
		consensus.fPowNoRetargeting = true;
		consensus.nPowKGWHeight = 15200; // same as mainnet
		consensus.nPowDGWHeight = 34140; // same as mainnet
		consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
		consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
		consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
		consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
		consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;
		consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
		consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
		consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].bit = 1;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nStartTime = 0;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nTimeout = 999999999999ULL;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].bit = 2;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nStartTime = 0;
		consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nTimeout = 999999999999ULL;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].bit = 3;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nStartTime = 0;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nTimeout = 999999999999ULL;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].bit = 4;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nStartTime = 0;
		consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nTimeout = 999999999999ULL;

		// The best chain should have at least this much work.
		consensus.nMinimumChainWork = uint256S("0x00");

		// By default assume that the signatures in ancestors of this block are valid.
		consensus.defaultAssumeValid = uint256S("0x00");

		pchMessageStart[0] = 0xfc;
		pchMessageStart[1] = 0xc1;
		pchMessageStart[2] = 0xb7;
		pchMessageStart[3] = 0xdc;
		nDefaultPort = 19899;
		nPruneAfterHeight = 1000;

		genesis = CreateGenesisBlock(1417713337, 1096447, 0x207fffff, 1, 50 * COIN, false);
		consensus.hashGenesisBlock = genesis.GetHash();
		// assert(consensus.hashGenesisBlock == uint256S("0x000008ca1832a4baf228eb1553c03d3a2c8e02399550dd6ea8d65cec3ef23d2e"));
		// assert(genesis.hashMerkleRoot == uint256S("0xe0028eb9648db56b1ac77cf090b99048a8007e2bb64b68f092c03c7f56a662c7"));

		vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
		vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

		fDefaultConsistencyChecks = true;
		fRequireStandard = false;
		fRequireRoutableExternalIP = false;
		fMineBlocksOnDemand = true;
		fAllowMultipleAddressesFromGroup = true;
		fAllowMultiplePorts = true;

		nFulfilledRequestExpireTime = 5 * 60; // fulfilled requests expire in 5 minutes
		nPoolMinParticipants = 3;
		nPoolMaxParticipants = 5;

		// privKey: cP4EKFyJsHT39LDqgdcB43Y3YXjNyjb5Fuas1GQSeAtjnZWmZEQK
		vSporkAddresses = { "yj949n1UH6fDhw6HtVE5VMj2iSTaSWBMcW" };
		nMinSporkKeys = 1;
		// regtest usually has no masternodes in most tests, so don't check for upgraged MNs
		fBIP9CheckMasternodesUpgraded = false;

		checkpointData = (CCheckpointData) {
			{
				{0, uint256S("0x000008ca1832a4baf228eb1553c03d3a2c8e02399550dd6ea8d65cec3ef23d2e")},
			}
		};

		chainTxData = ChainTxData{
			0,
			0,
			0
		};

		// Regtest LOKAL_Coin addresses start with 'y'
		base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 140);
		// Regtest LOKAL_Coin script addresses start with '8' or '9'
		base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 19);
		// Regtest private keys start with '9' or 'c' (Bitcoin defaults)
		base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);
		// Regtest LOKAL_Coin BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
		base58Prefixes[EXT_PUBLIC_KEY] = { 0x04, 0x35, 0x87, 0xCF };
		// Regtest LOKAL_Coin BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
		base58Prefixes[EXT_SECRET_KEY] = { 0x04, 0x35, 0x83, 0x94 };

		// Regtest LOKAL_Coin BIP44 coin type is '1' (All coin's testnet default)
		nExtCoinType = 1;

		// long living quorum params
		consensus.llmqs[Consensus::LLMQ_5_60] = llmq5_60;
		consensus.llmqs[Consensus::LLMQ_50_60] = llmq50_60;
		consensus.llmqTypeChainLocks = Consensus::LLMQ_5_60;
		consensus.llmqForInstaLOKAL = Consensus::LLMQ_5_60;
	}
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
	assert(globalChainParams);
	return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
	if (chain == CBaseChainParams::MAIN)
		return std::unique_ptr<CChainParams>(new CMainParams());
	else if (chain == CBaseChainParams::TESTNET)
		return std::unique_ptr<CChainParams>(new CTestNetParams());
	else if (chain == CBaseChainParams::DEVNET) {
		return std::unique_ptr<CChainParams>(new CDevNetParams());
	}
	else if (chain == CBaseChainParams::REGTEST)
		return std::unique_ptr<CChainParams>(new CRegTestParams());
	throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
	SelectBaseParams(network);
	globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout, int64_t nWindowSize, int64_t nThreshold)
{
	globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout, nWindowSize, nThreshold);
}

void UpdateDIP3Parameters(int nActivationHeight, int nEnforcementHeight)
{
	globalChainParams->UpdateDIP3Parameters(nActivationHeight, nEnforcementHeight);
}

void UpdateBudgetParameters(int nMasternodePaymentsStartBlock, int nBudgetPaymentsStartBlock, int nSuperblockStartBlock)
{
	globalChainParams->UpdateBudgetParameters(nMasternodePaymentsStartBlock, nBudgetPaymentsStartBlock, nSuperblockStartBlock);
}

void UpdateDevnetSubsidyAndDiffParams(int nMinimumDifficultyBlocks, int nHighSubsidyBlocks, int nHighSubsidyFactor)
{
	globalChainParams->UpdateSubsidyAndDiffParams(nMinimumDifficultyBlocks, nHighSubsidyBlocks, nHighSubsidyFactor);
}

void UpdateDevnetLLMQChainLocks(Consensus::LLMQType llmqType)
{
	globalChainParams->UpdateLLMQChainLocks(llmqType);
}
