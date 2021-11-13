/*    Copyright 2012 10gen Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects
 *    for all of the code used other than as permitted herein. If you modify
 *    file(s) with this exception, you may extend this exception to your
 *    version of the file(s), but you are not obligated to do so. If you do not
 *    wish to do so, delete this exception statement from your version. If you
 *    delete this exception statement from all source files in the program,
 *    then also delete it in the license file.
 */

#include "mongo/base/initializer.h"

#include "mongo/base/global_initializer.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/quick_exit.h"
#include <iostream>

namespace mongo {

Initializer::Initializer() {}
Initializer::~Initializer() {}

/*
ddd test Initializer::execute:OIDGeneration
ddd test Initializer::execute:ValidateLocale
ddd test Initializer::execute:EnableVersionInfo
ddd test Initializer::execute:GlobalLogManager
ddd test Initializer::execute:SystemInfo
ddd test Initializer::execute:TcmallocConfigurationDefaults
ddd test Initializer::execute:BeginStartupOptionHandling
ddd test Initializer::execute:BeginStartupOptionRegistration
ddd test Initializer::execute:BeginGeneralStartupOptionRegistration
ddd test Initializer::execute:MongodOptions_Register
ddd test Initializer::execute:EndGeneralStartupOptionRegistration
ddd test Initializer::execute:SASLOptions_Register
ddd test Initializer::execute:WiredTigerOptions_Register
ddd test Initializer::execute:EndStartupOptionRegistration
ddd test Initializer::execute:OptionsParseUseStrict
ddd test Initializer::execute:AuthzSchemaParameter
ddd test Initializer::execute:BeginStartupOptionParsing
ddd test Initializer::execute:StartupOptions_Parse
ddd test Initializer::execute:EndStartupOptionParsing
ddd test Initializer::execute:BeginStartupOptionValidation
ddd test Initializer::execute:WiredTigerOptions_Validate
ddd test Initializer::execute:FailPointRegistry
ddd test Initializer::execute:throwSockExcep
ddd test Initializer::execute:disableKeyGeneration
ddd test Initializer::execute:maxKeyRefreshWaitTimeOverrideMS
ddd test Initializer::execute:scheduleIntoPoolSpinsUntilThreadPoolShutsDown
ddd test Initializer::execute:failAsyncConfigChangeHook
ddd test Initializer::execute:checkForInterruptFail
ddd test Initializer::execute:disableAwaitDataForGetMoreCmd
ddd test Initializer::execute:keepCursorPinnedDuringGetMore
ddd test Initializer::execute:failApplyChunkOps
ddd test Initializer::execute:initialSyncHangCollectionClonerAfterHandlingBatchResponse
ddd test Initializer::execute:initialSyncHangDuringCollectionClone
ddd test Initializer::execute:crashOnShutdown
ddd test Initializer::execute:initialSyncHangAfterListCollections
ddd test Initializer::execute:stopReplProducer
ddd test Initializer::execute:rsSyncApplyStop
ddd test Initializer::execute:initialSyncHangBeforeFinish
ddd test Initializer::execute:failInitSyncWithBufferedEntriesLeft
ddd test Initializer::execute:blockHeartbeatStepdown
ddd test Initializer::execute:stepdownHangBeforePerformingPostMemberStateUpdateActions
ddd test Initializer::execute:blockHeartbeatReconfigFinish
ddd test Initializer::execute:setDistLockTimeout
ddd test Initializer::execute:hangBeforeLeavingCriticalSection
ddd test Initializer::execute:failMigrationCommit
ddd test Initializer::execute:failMigrationReceivedOutOfRangeOperation
ddd test Initializer::execute:failMigrationLeaveOrphans
ddd test Initializer::execute:migrateThreadHangAtStep6
ddd test Initializer::execute:migrateThreadHangAtStep4
ddd test Initializer::execute:migrateThreadHangAtStep3
ddd test Initializer::execute:transitionToPrimaryHangBeforeTakingGlobalExclusiveLock
ddd test Initializer::execute:migrateThreadHangAtStep2
ddd test Initializer::execute:doNotRefreshRecipientAfterCommit
ddd test Initializer::execute:onPrimaryTransactionalWrite
ddd test Initializer::execute:featureCompatibilityUpgrade
ddd test Initializer::execute:disableMaxSyncSourceLagSecs
ddd test Initializer::execute:failCollectionInserts
ddd test Initializer::execute:WTWriteConflictExceptionForReads
ddd test Initializer::execute:dummy
ddd test Initializer::execute:moveChunkHangAtStep4
ddd test Initializer::execute:suspendRangeDeletion
ddd test Initializer::execute:rsDelayHeartbeatResponse
ddd test Initializer::execute:disableSnapshotting
ddd test Initializer::execute:rollbackHangThenFailAfterWritingMinValid
ddd test Initializer::execute:moveChunkHangAtStep5
ddd test Initializer::execute:migrationCommitNetworkError
ddd test Initializer::execute:moveChunkHangAtStep7
ddd test Initializer::execute:maxTimeAlwaysTimeOut
ddd test Initializer::execute:initialSyncHangBeforeCollectionClone
ddd test Initializer::execute:setYieldAllLocksWait
ddd test Initializer::execute:WTWriteConflictException
ddd test Initializer::execute:hangAfterStartingIndexBuild
ddd test Initializer::execute:hangBeforeDatabaseUpgrade
ddd test Initializer::execute:hangAfterStartingIndexBuildUnlocked
ddd test Initializer::execute:moveChunkHangAtStep1
ddd test Initializer::execute:respondWithNotPrimaryInCommandDispatch
ddd test Initializer::execute:applyOpsPauseBetweenOperations
ddd test Initializer::execute:WTEmulateOutOfOrderNextIndexKey
ddd test Initializer::execute:moveChunkHangAtStep2
ddd test Initializer::execute:moveChunkHangAtStep6
ddd test Initializer::execute:moveChunkHangAtStep3
ddd test Initializer::execute:recordNeedsFetchFail
ddd test Initializer::execute:initialSyncHangBeforeGettingMissingDocument
ddd test Initializer::execute:skipCheckingForNotMasterInCommandDispatch
ddd test Initializer::execute:rsStopGetMore
ddd test Initializer::execute:NetworkInterfaceASIOasyncRunCommandFail
ddd test Initializer::execute:featureCompatibilityDowngrade
ddd test Initializer::execute:failAllRemoves
ddd test Initializer::execute:migrateThreadHangAtStep5
ddd test Initializer::execute:hangBeforeWaitingForWriteConcern
ddd test Initializer::execute:shutdownAtStartup
ddd test Initializer::execute:initialSyncHangBeforeCopyingDatabases
ddd test Initializer::execute:failAllUpdates
ddd test Initializer::execute:planExecutorAlwaysFails
ddd test Initializer::execute:mr_killop_test_fp
ddd test Initializer::execute:validateCmdCollectionNotValid
ddd test Initializer::execute:rsStopGetMoreCmd
ddd test Initializer::execute:failCollectionUpdates
ddd test Initializer::execute:initialSyncHangBeforeListCollections
ddd test Initializer::execute:rollbackHangBeforeFinish
ddd test Initializer::execute:hangBeforeLoggingCreateCollection
ddd test Initializer::execute:crashAfterStartingIndexBuild
ddd test Initializer::execute:maxTimeNeverTimeOut
ddd test Initializer::execute:failInitialSyncWithBadHost
ddd test Initializer::execute:rollbackHangBeforeStart
ddd test Initializer::execute:migrationCommitVersionError
ddd test Initializer::execute:allocateDiskFull
ddd test Initializer::execute:WTPausePrimaryOplogDurabilityLoop
ddd test Initializer::execute:skipIndexCreateFieldNameValidation
ddd test Initializer::execute:migrateThreadHangAtStep1
ddd test Initializer::execute:setYieldAllLocksHang
ddd test Initializer::execute:failReceivedGetmore
ddd test Initializer::execute:failAllInserts
ddd test Initializer::execute:setAutoGetCollectionWait
ddd test Initializer::execute:AllFailPointsRegistered
ddd test Initializer::execute:MongodOptions
ddd test Initializer::execute:EndStartupOptionValidation
ddd test Initializer::execute:BeginStartupOptionSetup
ddd test Initializer::execute:ServerOptions_Setup
ddd test Initializer::execute:EndStartupOptionSetup
ddd test Initializer::execute:BeginStartupOptionStorage
ddd test Initializer::execute:WiredTigerOptions_Store
ddd test Initializer::execute:SASLOptions_Store
ddd test Initializer::execute:MongodOptions_Store
ddd test Initializer::execute:EndStartupOptionStorage
ddd test Initializer::execute:EndStartupOptionHandling
ddd test Initializer::execute:ForkServer
ddd test Initializer::execute:StartHeapProfiling
ddd test Initializer::execute:ServerLogRedirection
ddd test Initializer::execute:default
ddd test Initializer::execute:SystemTickSourceInit
ddd test Initializer::execute:ThreadNameInitializer
ddd test Initializer::execute:RamLogCatalog
ddd test Initializer::execute:LogstreamBuilder
ddd test Initializer::execute:EnsureIosBaseInitConstructed
ddd test Initializer::execute:addAliasToDocSourceParserMap_sortByCount
ddd test Initializer::execute:addToGranularityRounderMap_R10
ddd test Initializer::execute:addToDocSourceParserMap_replaceRoot
ddd test Initializer::execute:addToDocSourceParserMap_currentOp
ddd test Initializer::execute:addToExpressionParserMap_and
ddd test Initializer::execute:addAliasToDocSourceParserMap_count
ddd test Initializer::execute:addToExpressionParserMap_lt
ddd test Initializer::execute:LoadICUData
ddd test Initializer::execute:addToExpressionParserMap_meta
ddd test Initializer::execute:addAliasToDocSourceParserMap_changeStream
ddd test Initializer::execute:GenerateInstanceId
ddd test Initializer::execute:addToExpressionParserMap_indexOfBytes
ddd test Initializer::execute:InitializeCollectionInfoCacheFactory
ddd test Initializer::execute:ExtractSOMap
ddd test Initializer::execute:RegisterClearLogCmd
ddd test Initializer::execute:addToDocSourceParserMap_addFields
ddd test Initializer::execute:addToExpressionParserMap_lte
ddd test Initializer::execute:addToDocSourceParserMap__internalInhibitOptimization
ddd test Initializer::execute:addToDocSourceParserMap_group
ddd test Initializer::execute:addToExpressionParserMap_dateFromString
ddd test Initializer::execute:addToExpressionParserMap_toLower
ddd test Initializer::execute:addToExpressionParserMap_mod
ddd test Initializer::execute:addToGranularityRounderMap_E96
ddd test Initializer::execute:languageDanV1
ddd test Initializer::execute:addToExpressionParserMap_filter
ddd test Initializer::execute:InitializeMultiIndexBlockFactory
ddd test Initializer::execute:languageItalianV1
ddd test Initializer::execute:addToDocSourceParserMap_sort
ddd test Initializer::execute:languageTurkishV1
ddd test Initializer::execute:languageTurV1
ddd test Initializer::execute:languageTrV1
ddd test Initializer::execute:languageSwedishV1
ddd test Initializer::execute:languageSpanishV1
ddd test Initializer::execute:languageGerV1
ddd test Initializer::execute:languageFrV1
ddd test Initializer::execute:languageRonV1
ddd test Initializer::execute:languageEnglishV1
ddd test Initializer::execute:languageEngV1
ddd test Initializer::execute:languagePortugueseV1
ddd test Initializer::execute:languageSvV1
ddd test Initializer::execute:languageNoneV1
ddd test Initializer::execute:languageEslV1
ddd test Initializer::execute:languageDaV1
ddd test Initializer::execute:languageSpaV1
ddd test Initializer::execute:FTSRegisterV2LanguagesAndLater
ddd test Initializer::execute:languageFinnishV1
ddd test Initializer::execute:languageGermanV1
ddd test Initializer::execute:languageDeV1
ddd test Initializer::execute:languageEnV1
ddd test Initializer::execute:languageHungarianV1
ddd test Initializer::execute:languageRusV1
ddd test Initializer::execute:languageFrenchV1
ddd test Initializer::execute:languageEsV1
ddd test Initializer::execute:languageSweV1
ddd test Initializer::execute:languageRussianV1
ddd test Initializer::execute:languageDeuV1
ddd test Initializer::execute:languageDutV1
ddd test Initializer::execute:languageHuV1
ddd test Initializer::execute:languageHunV1
ddd test Initializer::execute:languageDutchV1
ddd test Initializer::execute:languageItV1
ddd test Initializer::execute:languageItaV1
ddd test Initializer::execute:languageNorV1
ddd test Initializer::execute:languageNlV1
ddd test Initializer::execute:languageNoV1
ddd test Initializer::execute:languagePorterV1
ddd test Initializer::execute:languageFinV1
ddd test Initializer::execute:languageNldV1
ddd test Initializer::execute:languageNorwegianV1
ddd test Initializer::execute:languageDanishV1
ddd test Initializer::execute:languagePtV1
ddd test Initializer::execute:languagePorV1
ddd test Initializer::execute:languageRoV1
ddd test Initializer::execute:languageRomanianV1
ddd test Initializer::execute:languageFreV1
ddd test Initializer::execute:languageRuV1
ddd test Initializer::execute:languageFiV1
ddd test Initializer::execute:languageFraV1
ddd test Initializer::execute:languageRumV1
ddd test Initializer::execute:FTSAllLanguagesRegistered
ddd test Initializer::execute:addToAccumulatorFactoryMap_stdDevPop
ddd test Initializer::execute:addToDocSourceParserMap_skip
ddd test Initializer::execute:InitializeJournalingParams
ddd test Initializer::execute:FTSRegisterLanguageAliases
ddd test Initializer::execute:NoopMessageCompressorInit
ddd test Initializer::execute:AuthIndexKeyPatterns
ddd test Initializer::execute:RegisterRefreshLogicalSessionCacheNowCommand
ddd test Initializer::execute:addToExpressionParserMap_substr
ddd test Initializer::execute:addToExpressionParserMap_toUpper
ddd test Initializer::execute:RecordBlockSupported
ddd test Initializer::execute:addToDocSourceParserMap_project
ddd test Initializer::execute:addToExpressionParserMap_or
ddd test Initializer::execute:SetupInternalSecurityUser
ddd test Initializer::execute:addToExpressionParserMap_month
ddd test Initializer::execute:InitializeFixIndexKeyImpl
ddd test Initializer::execute:addToExpressionParserMap_allElementsTrue
ddd test Initializer::execute:addToExpressionParserMap_multiply
ddd test Initializer::execute:SetInitRsOplogBackgroundThreadCallback
ddd test Initializer::execute:ClusterBalancerControlCommands
ddd test Initializer::execute:addToDocSourceParserMap_listLocalSessions
ddd test Initializer::execute:addToExpressionParserMap_ifNull
ddd test Initializer::execute:addToDocSourceParserMap_redact
ddd test Initializer::execute:addToDocSourceParserMap__internalSplitPipeline
ddd test Initializer::execute:addToDocSourceParserMap_graphLookup
ddd test Initializer::execute:addToExpressionParserMap_week
ddd test Initializer::execute:addToDocSourceParserMap_collStats
ddd test Initializer::execute:SetGlobalEnvironment
ddd test Initializer::execute:addToExpressionParserMap_pow
ddd test Initializer::execute:JavascriptPrintDomain
ddd test Initializer::execute:RegisterJournalLatencyTestCmd
ddd test Initializer::execute:WiredTigerEngineInit
ddd test Initializer::execute:InitializeIndexCatalogFactory
ddd test Initializer::execute:addToExpressionParserMap_floor
ddd test Initializer::execute:SSLManager
ddd test Initializer::execute:CreateReplicationManager
ddd test Initializer::execute:EphemeralForTestEngineInit
ddd test Initializer::execute:SetupPlanCacheCommands
ddd test Initializer::execute:addToDocSourceParserMap_listSessions
ddd test Initializer::execute:addToDocSourceParserMap_indexStats
ddd test Initializer::execute:InitializeConnectionPools
ddd test Initializer::execute:RegisterFaultInjectCmd
ddd test Initializer::execute:addToExpressionParserMap_objectToArray
ddd test Initializer::execute:RegisterEmptyCappedCmd
ddd test Initializer::execute:addToExpressionParserMap_switch
ddd test Initializer::execute:InitializeDatabaseHolderFactory
ddd test Initializer::execute:InitializeDbHolderimpl
ddd test Initializer::execute:periodicNoopIntervalSecs
ddd test Initializer::execute:addToAccumulatorFactoryMap_avg
ddd test Initializer::execute:addToAccumulatorFactoryMap_stdDevSamp
ddd test Initializer::execute:RegisterIsSelfCommand
ddd test Initializer::execute:RegisterStageDebugCmd
ddd test Initializer::execute:TCMallocThreadIdleListener
ddd test Initializer::execute:DevNullEngineInit
ddd test Initializer::execute:addToExpressionParserMap_isoWeekYear
ddd test Initializer::execute:CreateAuthorizationExternalStateFactory
ddd test Initializer::execute:addToExpressionParserMap_dateToParts
ddd test Initializer::execute:addToExpressionParserMap_not
ddd test Initializer::execute:RegisterCpuLoadCmd
ddd test Initializer::execute:InitializeAdvanceClusterTimePrivilegeVector
ddd test Initializer::execute:RegisterDbCheckCmd
ddd test Initializer::execute:InitializeParseValidationLevelImpl
ddd test Initializer::execute:addAliasToDocSourceParserMap_bucket
ddd test Initializer::execute:ZlibMessageCompressorInit
ddd test Initializer::execute:SetupIndexFilterCommands
ddd test Initializer::execute:RegisterAppendOpLogNoteCmd
ddd test Initializer::execute:SaslClientAuthenticateFunction
ddd test Initializer::execute:SetServerLogContextFunction
ddd test Initializer::execute:RegisterReplSetTestCmd
ddd test Initializer::execute:NativeSaslServerCore
ddd test Initializer::execute:PreSaslCommands
ddd test Initializer::execute:addToExpressionParserMap_substrCP
ddd test Initializer::execute:addToExpressionParserMap_mergeObjects
ddd test Initializer::execute:SetupDottedNames
ddd test Initializer::execute:addToExpressionParserMap_trunc
ddd test Initializer::execute:addToDocSourceParserMap_lookup
ddd test Initializer::execute:addToDocSourceParserMap_facet
ddd test Initializer::execute:addToExpressionParserMap_setIsSubset
ddd test Initializer::execute:RegisterReapLogicalSessionCacheNowCommand
ddd test Initializer::execute:initialSyncOplogBuffer
ddd test Initializer::execute:InitializeUserCreateNSImpl
ddd test Initializer::execute:InitializeDropAllDatabasesExceptLocalImpl
ddd test Initializer::execute:addToDocSourceParserMap_listLocalCursors
ddd test Initializer::execute:addToDocSourceParserMap_sample
ddd test Initializer::execute:addToDocSourceParserMap_out
ddd test Initializer::execute:MungeUmask
ddd test Initializer::execute:PostSaslCommands
ddd test Initializer::execute:addToExpressionParserMap_isoDayOfWeek
ddd test Initializer::execute:addToDocSourceParserMap_mergeCursors
ddd test Initializer::execute:addToExpressionParserMap_add
ddd test Initializer::execute:addToExpressionParserMap_isArray
ddd test Initializer::execute:CreateDiagnosticDataCommand
ddd test Initializer::execute:GlobalCursorIdCache
ddd test Initializer::execute:GlobalCursorManager
ddd test Initializer::execute:RegisterShortCircuitExitHandler
ddd test Initializer::execute:addToExpressionParserMap_stdDevPop
ddd test Initializer::execute:InitializeCollectionFactory
ddd test Initializer::execute:addToExpressionParserMap_minute
ddd test Initializer::execute:addToExpressionParserMap_year
ddd test Initializer::execute:addToDocSourceParserMap_match
ddd test Initializer::execute:InitializeIndexCatalogIndexIteratorFactory
ddd test Initializer::execute:InitializePrepareInsertDeleteOptionsImpl
ddd test Initializer::execute:InitializeIndexCatalogEntryFactory
ddd test Initializer::execute:addToDocSourceParserMap_unwind
ddd test Initializer::execute:addToDocSourceParserMap_limit
ddd test Initializer::execute:addToGranularityRounderMap_R5
ddd test Initializer::execute:addToGranularityRounderMap_R20
ddd test Initializer::execute:addToGranularityRounderMap_R40
ddd test Initializer::execute:addToGranularityRounderMap_E6
ddd test Initializer::execute:addToGranularityRounderMap_E24
ddd test Initializer::execute:addToGranularityRounderMap_E48
ddd test Initializer::execute:addToGranularityRounderMap_E192
ddd test Initializer::execute:addToExpressionParserMap_setDifference
ddd test Initializer::execute:addToAccumulatorFactoryMap_addToSet
ddd test Initializer::execute:addToExpressionParserMap_avg
ddd test Initializer::execute:addToAccumulatorFactoryMap_first
ddd test Initializer::execute:addToExpressionParserMap_map
ddd test Initializer::execute:addToAccumulatorFactoryMap_last
ddd test Initializer::execute:addToAccumulatorFactoryMap_max
ddd test Initializer::execute:addToExpressionParserMap_const
ddd test Initializer::execute:addToAccumulatorFactoryMap_min
ddd test Initializer::execute:addToExpressionParserMap_max
ddd test Initializer::execute:addToExpressionParserMap_min
ddd test Initializer::execute:addToExpressionParserMap_cond
ddd test Initializer::execute:addToAccumulatorFactoryMap_push
ddd test Initializer::execute:addToExpressionParserMap_stdDevSamp
ddd test Initializer::execute:addToAccumulatorFactoryMap_sum
ddd test Initializer::execute:RegisterHashEltCmd
ddd test Initializer::execute:addToExpressionParserMap_sum
ddd test Initializer::execute:addToAccumulatorFactoryMap_mergeObjects
ddd test Initializer::execute:NativeSaslClientContext
ddd test Initializer::execute:addToExpressionParserMap_indexOfCP
ddd test Initializer::execute:MatchExpressionParser
ddd test Initializer::execute:CreateAuthorizationManager
ddd test Initializer::execute:addToGranularityRounderMap_1_2_5
ddd test Initializer::execute:FTSIndexFormat
ddd test Initializer::execute:SnappyMessageCompressorInit
ddd test Initializer::execute:AllCompressorsRegistered
ddd test Initializer::execute:InitializeParseValidationActionImpl
ddd test Initializer::execute:RegisterTopCommand
ddd test Initializer::execute:addToExpressionParserMap_ceil
ddd test Initializer::execute:addToExpressionParserMap_isoWeek
ddd test Initializer::execute:addToExpressionParserMap_divide
ddd test Initializer::execute:addToExpressionParserMap_gte
ddd test Initializer::execute:CreateCollatorFactory
ddd test Initializer::execute:StopWords
ddd test Initializer::execute:addToExpressionParserMap_hour
ddd test Initializer::execute:addToExpressionParserMap_dayOfYear
ddd test Initializer::execute:MMAPV1EngineInit
ddd test Initializer::execute:RegisterSnapshotManagementCommands
ddd test Initializer::execute:addToExpressionParserMap_setUnion
ddd test Initializer::execute:addToDocSourceParserMap_geoNear
ddd test Initializer::execute:AuthorizationBuiltinRoles
ddd test Initializer::execute:ModifierTable
ddd test Initializer::execute:addToGranularityRounderMap_E12
ddd test Initializer::execute:addToExpressionParserMap_literal
ddd test Initializer::execute:PathlessOperatorMap
ddd test Initializer::execute:addToExpressionParserMap_strLenCP
ddd test Initializer::execute:addToExpressionParserMap_dayOfMonth
ddd test Initializer::execute:InitializeDatabaseFactory
ddd test Initializer::execute:addToExpressionParserMap_millisecond
ddd test Initializer::execute:addToExpressionParserMap_dayOfWeek
ddd test Initializer::execute:addToExpressionParserMap_second
ddd test Initializer::execute:SecureAllocator
ddd test Initializer::execute:addToExpressionParserMap_abs
ddd test Initializer::execute:addToGranularityRounderMap_POWERSOF2
ddd test Initializer::execute:addToExpressionParserMap_anyElementTrue
ddd test Initializer::execute:addToExpressionParserMap_size
ddd test Initializer::execute:addToExpressionParserMap_arrayElemAt
ddd test Initializer::execute:S2CellIdInit
ddd test Initializer::execute:addToExpressionParserMap_arrayToObject
ddd test Initializer::execute:addToExpressionParserMap_setIntersection
ddd test Initializer::execute:addToExpressionParserMap_cmp
ddd test Initializer::execute:addToExpressionParserMap_eq
ddd test Initializer::execute:addToExpressionParserMap_gt
ddd test Initializer::execute:addToExpressionParserMap_ne
ddd test Initializer::execute:addToExpressionParserMap_concatArrays
ddd test Initializer::execute:addToExpressionParserMap_concat
ddd test Initializer::execute:addToExpressionParserMap_dateFromParts
ddd test Initializer::execute:addToExpressionParserMap_dateToString
ddd test Initializer::execute:addToExpressionParserMap_exp
ddd test Initializer::execute:addToExpressionParserMap_let
ddd test Initializer::execute:addToExpressionParserMap_in
ddd test Initializer::execute:addToExpressionParserMap_indexOfArray
ddd test Initializer::execute:addToExpressionParserMap_ln
ddd test Initializer::execute:addToExpressionParserMap_log
ddd test Initializer::execute:addToExpressionParserMap_log10
ddd test Initializer::execute:addToExpressionParserMap_range
ddd test Initializer::execute:addToExpressionParserMap_reduce
ddd test Initializer::execute:addToExpressionParserMap_reverseArray
ddd test Initializer::execute:addToExpressionParserMap_setEquals
ddd test Initializer::execute:addToExpressionParserMap_slice
ddd test Initializer::execute:addToDocSourceParserMap_bucketAuto
ddd test Initializer::execute:LoadTimeZoneDB
ddd test Initializer::execute:addToExpressionParserMap_split
ddd test Initializer::execute:addToExpressionParserMap_sqrt
ddd test Initializer::execute:addToGranularityRounderMap_R80
ddd test Initializer::execute:addToExpressionParserMap_strcasecmp
ddd test Initializer::execute:addToExpressionParserMap_substrBytes
ddd test Initializer::execute:addToExpressionParserMap_strLenBytes
ddd test Initializer::execute:addToExpressionParserMap_subtract
ddd test Initializer::execute:addToExpressionParserMap_type
ddd test Initializer::execute:addToExpressionParserMap_zip
ddd test Initializer::execute:SetWiredTigerExtensions
ddd test Initializer::execute:S2RegionCovererInit
ddd test Initializer::execute:replSettingsCheck
ddd test Initializer::execute:SetWiredTigerCustomizationHooks
ddd test Initializer::execute:InitializeDropDatabaseImpl
ddd test Initializer::execute:SetEncryptionHooks
*/
//args命令行参数，env系统环境变量信息  runGlobalInitializers 中调用执行
Status Initializer::execute(const InitializerContext::ArgumentVector& args,
                            const InitializerContext::EnvironmentMap& env) const {
    std::vector<std::string> sortedNodes;
    Status status = _graph.topSort(&sortedNodes);
    if (Status::OK() != status)
        return status;
	
    InitializerContext context(args, env);

    for (size_t i = 0; i < sortedNodes.size(); ++i) {
		/*
		std::cout << "ddd test Initializer::execute:" << sortedNodes[i] << std::endl;
        */
        InitializerFunction fn = _graph.getInitializerFunction(sortedNodes[i]);
        if (!fn) {
            return Status(ErrorCodes::InternalError,
                          "topSort returned a node that has no associated function: \"" +
                              sortedNodes[i] + '"');
        }
        try {
            status = fn(&context);
        } catch (const DBException& xcp) {
            return xcp.toStatus();
        }

        if (Status::OK() != status)
            return status;
    }
    return Status::OK();
}

Status runGlobalInitializers(const InitializerContext::ArgumentVector& args,
                             const InitializerContext::EnvironmentMap& env) {
    return getGlobalInitializer().execute(args, env);
}

/*
主要解析命令行，即启动参数，其中会读取配置文件，解析参数并赋值给变量，
比如“cpu”，“noauth”，“quota”，“repair path”，“nohints”等。
*/ //mongoSMain  mongoDbMain调用
Status runGlobalInitializers(int argc, const char* const* argv, const char* const* envp) {
    InitializerContext::ArgumentVector args(argc);
    std::copy(argv, argv + argc, args.begin()); //拷贝命令行参数到args数组

    InitializerContext::EnvironmentMap env;

    if (envp) { //envp存储的是export设置的环境变量信息存入env map表中
        for (; *envp; ++envp) {
            const char* firstEqualSign = strchr(*envp, '=');
            if (!firstEqualSign) {
                return Status(ErrorCodes::BadValue, "malformed environment block");
            }
            env[std::string(*envp, firstEqualSign)] = std::string(firstEqualSign + 1);
			//std::cout << "ddd test runGlobalInitializers:" << env[std::string(*envp, firstEqualSign)] << ":" << std::string(firstEqualSign + 1) << std::endl;
        }
    }

    return runGlobalInitializers(args, env);
}

void runGlobalInitializersOrDie(int argc, const char* const* argv, const char* const* envp) {
    Status status = runGlobalInitializers(argc, argv, envp);
    if (!status.isOK()) {
        std::cerr << "Failed global initialization: " << status << std::endl;
        quickExit(1);
    }
}

}  // namespace mongo
