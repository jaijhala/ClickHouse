#pragma once

#include "config.h"

#if USE_HDFS

#include <Processors/ISource.h>
#include <Storages/IStorage.h>
#include <Storages/Cache/SchemaCache.h>
#include <Poco/URI.h>

namespace DB
{

class IInputFormat;

/**
 * This class represents table engine for external hdfs files.
 * Read method is supported for now.
 */
class StorageHDFS final : public IStorage, WithContext
{
public:
    struct PathInfo
    {
        time_t last_mod_time;
        size_t size;
    };

    struct PathWithInfo
    {
        String path;
        std::optional<PathInfo> info;
    };

    StorageHDFS(
        const String & uri_,
        const StorageID & table_id_,
        const String & format_name_,
        const ColumnsDescription & columns_,
        const ConstraintsDescription & constraints_,
        const String & comment,
        ContextPtr context_,
        const String & compression_method_ = "",
        bool distributed_processing_ = false,
        ASTPtr partition_by = nullptr);

    String getName() const override { return "HDFS"; }

    Pipe read(
        const Names & column_names,
        const StorageSnapshotPtr & storage_snapshot,
        SelectQueryInfo & query_info,
        ContextPtr context,
        QueryProcessingStage::Enum processed_stage,
        size_t max_block_size,
        size_t num_streams) override;

    SinkToStoragePtr write(const ASTPtr & query, const StorageMetadataPtr & metadata_snapshot, ContextPtr context, bool async_insert) override;

    void truncate(
        const ASTPtr & query,
        const StorageMetadataPtr & metadata_snapshot,
        ContextPtr local_context,
        TableExclusiveLockHolder &) override;

    NamesAndTypesList getVirtuals() const override;

    bool supportsPartitionBy() const override { return true; }

    /// Check if the format is column-oriented.
    /// Is is useful because column oriented formats could effectively skip unknown columns
    /// So we can create a header of only required columns in read method and ask
    /// format to read only them. Note: this hack cannot be done with ordinary formats like TSV.
    bool supportsSubsetOfColumns() const override;

    static ColumnsDescription getTableStructureFromData(
        const String & format,
        const String & uri,
        const String & compression_method,
        ContextPtr ctx);

    static SchemaCache & getSchemaCache(const ContextPtr & ctx);

protected:
    friend class HDFSSource;

private:
    static std::optional<ColumnsDescription> tryGetColumnsFromCache(
        const std::vector<StorageHDFS::PathWithInfo> & paths_with_info,
        const String & uri_without_path,
        const String & format_name,
        const ContextPtr & ctx);

    static void addColumnsToCache(
        const std::vector<StorageHDFS::PathWithInfo> & paths,
        const String & uri_without_path,
        const ColumnsDescription & columns,
        const String & format_name,
        const ContextPtr & ctx);

    std::vector<String> uris;
    String format_name;
    String compression_method;
    const bool distributed_processing;
    ASTPtr partition_by;
    bool is_path_with_globs;
    NamesAndTypesList virtual_columns;

    Poco::Logger * log = &Poco::Logger::get("StorageHDFS");
};

class PullingPipelineExecutor;

class HDFSSource : public ISource, WithContext
{
public:
    class DisclosedGlobIterator
    {
        public:
            DisclosedGlobIterator(ContextPtr context_, const String & uri_);
            StorageHDFS::PathWithInfo next();
        private:
            class Impl;
            /// shared_ptr to have copy constructor
            std::shared_ptr<Impl> pimpl;
    };

    class URISIterator
    {
        public:
            URISIterator(const std::vector<String> & uris_, ContextPtr context);
            StorageHDFS::PathWithInfo next();
        private:
            class Impl;
            /// shared_ptr to have copy constructor
            std::shared_ptr<Impl> pimpl;
    };

    using IteratorWrapper = std::function<StorageHDFS::PathWithInfo()>;
    using StorageHDFSPtr = std::shared_ptr<StorageHDFS>;

    static Block getHeader(Block sample_block, const std::vector<NameAndTypePair> & requested_virtual_columns);

    HDFSSource(
        StorageHDFSPtr storage_,
        const Block & block_for_format_,
        const std::vector<NameAndTypePair> & requested_virtual_columns_,
        ContextPtr context_,
        UInt64 max_block_size_,
        std::shared_ptr<IteratorWrapper> file_iterator_,
        ColumnsDescription columns_description_);

    String getName() const override;

    Chunk generate() override;

private:
    StorageHDFSPtr storage;
    Block block_for_format;
    std::vector<NameAndTypePair> requested_virtual_columns;
    UInt64 max_block_size;
    std::shared_ptr<IteratorWrapper> file_iterator;
    ColumnsDescription columns_description;

    std::unique_ptr<ReadBuffer> read_buf;
    std::shared_ptr<IInputFormat> input_format;
    std::unique_ptr<QueryPipeline> pipeline;
    std::unique_ptr<PullingPipelineExecutor> reader;
    String current_path;

    UInt64 total_rows_approx_max = 0;
    size_t total_rows_count_times = 0;
    UInt64 total_rows_approx_accumulated = 0;
    size_t total_files_size = 0;

    /// Recreate ReadBuffer and PullingPipelineExecutor for each file.
    bool initialize();
};
}

#endif
