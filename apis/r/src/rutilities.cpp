
// we currently get deprecation warnings by default which are noisy
#ifndef TILEDB_NO_API_DEPRECATION_WARNINGS
#define TILEDB_NO_API_DEPRECATION_WARNINGS
#endif

#include <Rcpp.h>                 // for R interface to C++
#include <nanoarrow/nanoarrow.h>  // for C interface to Arrow
#include <nanoarrow/r.h>          // for C interface to Arrow (via R package)
#include <tiledbsoma/reindexer/reindexer.h>
#include <RcppInt64>  // for fromInteger64
#include <tiledbsoma/tiledbsoma>

#include "rutilities.h"  // local declarations
#include "xptr-utils.h"  // xptr taggging utilitie

namespace tdbs = tiledbsoma;

void apply_dim_points(
    tdbs::ManagedQuery* mq,
    std::unordered_map<std::string, std::shared_ptr<tiledb::Dimension>>&
        name2dim,
    Rcpp::List lst) {
    std::vector<std::string> colnames = lst.attr("names");
    for (auto& nm : colnames) {
        auto dm = name2dim[nm];
        auto tp = dm->type();
        bool suitable = false;
        if (tp == TILEDB_UINT64) {
            Rcpp::NumericVector payload = lst[nm];
            std::vector<int64_t> iv = Rcpp::fromInteger64(payload, false);
            std::vector<uint64_t> uv(iv.size());
            const std::pair<uint64_t, uint64_t> pr = dm->domain<uint64_t>();
            for (size_t i = 0; i < iv.size(); i++) {
                uv[i] = static_cast<uint64_t>(iv[i]);
                if (uv[i] >= pr.first && uv[i] <= pr.second) {
                    mq->select_point<uint64_t>(
                        nm, uv[i]);  // bonked when use with vector
                    tdbs::LOG_INFO(fmt::format("[apply_dim_points] Applying dim point {} on {}",
                        uv[i],
                        nm));
                    suitable = true;
                }
            }
        } else if (tp == TILEDB_INT64) {
            Rcpp::NumericVector payload = lst[nm];
            std::vector<int64_t> iv = Rcpp::fromInteger64(payload, false);
            const std::pair<int64_t, int64_t> pr = dm->domain<int64_t>();
            for (size_t i = 0; i < iv.size(); i++) {
                if (iv[i] >= pr.first && iv[i] <= pr.second) {
                    mq->select_point<int64_t>(nm, iv[i]);
                    tdbs::LOG_DEBUG(fmt::format("[apply_dim_points] Applying dim point {} on {}",
                        iv[i],
                        nm));
                    suitable = true;
                }
            }
        } else if (tp == TILEDB_FLOAT32) {
            Rcpp::NumericVector payload = lst[nm];
            const std::pair<float, float> pr = dm->domain<float>();
            for (R_xlen_t i = 0; i < payload.size(); i++) {
                float v = static_cast<float>(payload[i]);
                if (v >= pr.first && v <= pr.second) {
                    mq->select_point<float>(nm, v);
                    tdbs::LOG_DEBUG(fmt::format("[apply_dim_points] Applying dim point {} on {}",
                        v,
                        nm));
                    suitable = true;
                }
            }
        } else if (tp == TILEDB_FLOAT64) {
            Rcpp::NumericVector payload = lst[nm];
            const std::pair<double, double> pr = dm->domain<double>();
            for (R_xlen_t i = 0; i < payload.size(); i++) {
                if (payload[i] >= pr.first && payload[i] <= pr.second) {
                    mq->select_point<double>(nm, payload[i]);
                    tdbs::LOG_DEBUG(fmt::format("[apply_dim_points] Applying dim point {} on {}",
                        payload[i],
                        nm));
                    suitable = true;
                }
            }
        } else if (tp == TILEDB_INT32) {
            Rcpp::IntegerVector payload = lst[nm];
            const std::pair<int32_t, int32_t> pr = dm->domain<int32_t>();
            for (R_xlen_t i = 0; i < payload.size(); i++) {
                if (payload[i] >= pr.first && payload[i] <= pr.second) {
                    mq->select_point<int32_t>(nm, payload[i]);
                    tdbs::LOG_DEBUG(fmt::format("[apply_dim_points] Applying dim point {} on {}",
                        payload[i],
                        nm));
                    suitable = true;
                }
            }
        } else {
            Rcpp::stop(
                "Currently unsupported type: ", tiledb::impl::to_str(tp));
        }
        if (!suitable) {
            Rcpp::stop(
                "Unsuitable dim points on dimension '%s' with domain %s",
                nm,
                dm->domain_to_str());
        }
    }
}

void apply_dim_ranges(
    tdbs::ManagedQuery* mq,
    std::unordered_map<std::string, std::shared_ptr<tiledb::Dimension>>&
        name2dim,
    Rcpp::List lst) {
    std::vector<std::string> colnames = lst.attr("names");
    for (auto& nm : colnames) {
        auto dm = name2dim[nm];
        auto tp = dm->type();
        bool suitable = false;
        if (tp == TILEDB_UINT64) {
            Rcpp::NumericMatrix mm = lst[nm];
            Rcpp::NumericMatrix::Column lo = mm.column(
                0);  // works as proxy for int and float types
            Rcpp::NumericMatrix::Column hi = mm.column(
                1);  // works as proxy for int and float types
            std::vector<std::pair<uint64_t, uint64_t>> vp(mm.nrow());
            const std::pair<uint64_t, uint64_t> pr = dm->domain<uint64_t>();
            for (int i = 0; i < mm.nrow(); i++) {
                uint64_t l = static_cast<uint64_t>(Rcpp::fromInteger64(lo[i]));
                uint64_t h = static_cast<uint64_t>(Rcpp::fromInteger64(hi[i]));
                vp[i] = std::make_pair(
                    std::max(l, pr.first), std::min(h, pr.second));
                tdbs::LOG_DEBUG(fmt::format("[apply_dim_ranges] Applying dim point {} on {} with {} - "
                    "{}",
                    i,
                    nm,
                    l,
                    h));
                suitable = l < pr.second &&
                           h > pr.first;  // lower must be less than max, higher
                                          // more than min
            }
            if (suitable)
                mq->select_ranges<uint64_t>(nm, vp);
        } else if (tp == TILEDB_INT64) {
            Rcpp::NumericMatrix mm = lst[nm];
            std::vector<int64_t> lo = Rcpp::fromInteger64(mm.column(0), false);
            std::vector<int64_t> hi = Rcpp::fromInteger64(mm.column(1), false);
            std::vector<std::pair<int64_t, int64_t>> vp(mm.nrow());
            const std::pair<int64_t, int64_t> pr = dm->domain<int64_t>();
            for (int i = 0; i < mm.nrow(); i++) {
                vp[i] = std::make_pair(
                    std::max(lo[i], pr.first), std::min(hi[i], pr.second));
                tdbs::LOG_DEBUG(fmt::format("[apply_dim_ranges] Applying dim point {} on {} with {} - "
                    "{}",
                    i,
                    nm,
                    lo[i],
                    hi[i]));
                suitable = lo[i] < pr.second &&
                           hi[i] > pr.first;  // lower must be less than max,
                                              // higher more than min
            }
            if (suitable)
                mq->select_ranges<int64_t>(nm, vp);
        } else if (tp == TILEDB_FLOAT32) {
            Rcpp::NumericMatrix mm = lst[nm];
            Rcpp::NumericMatrix::Column lo = mm.column(
                0);  // works as proxy for int and float types
            Rcpp::NumericMatrix::Column hi = mm.column(
                1);  // works as proxy for int and float types
            std::vector<std::pair<float, float>> vp(mm.nrow());
            const std::pair<float, float> pr = dm->domain<float>();
            for (int i = 0; i < mm.nrow(); i++) {
                float l = static_cast<float>(lo[i]);
                float h = static_cast<float>(hi[i]);
                vp[i] = std::make_pair(
                    std::max(l, pr.first), std::min(h, pr.second));
                tdbs::LOG_DEBUG(fmt::format("[apply_dim_ranges] Applying dim point {} on {} with {} - "
                    "{}",
                    i,
                    nm,
                    l,
                    h));
                suitable = l < pr.second &&
                           h > pr.first;  // lower must be less than max, higher
                                          // more than min
            }
            if (suitable)
                mq->select_ranges<float>(nm, vp);
        } else if (tp == TILEDB_FLOAT64) {
            Rcpp::NumericMatrix mm = lst[nm];
            Rcpp::NumericMatrix::Column lo = mm.column(
                0);  // works as proxy for int and float types
            Rcpp::NumericMatrix::Column hi = mm.column(
                1);  // works as proxy for int and float types
            std::vector<std::pair<double, double>> vp(mm.nrow());
            const std::pair<double, double> pr = dm->domain<double>();
            for (int i = 0; i < mm.nrow(); i++) {
                vp[i] = std::make_pair(
                    std::max(lo[i], pr.first), std::min(hi[i], pr.second));
                tdbs::LOG_DEBUG(fmt::format("[apply_dim_ranges] Applying dim point {} on {} with {} - "
                    "{}",
                    i,
                    nm,
                    lo[i],
                    hi[i]));
                suitable = lo[i] < pr.second &&
                           hi[i] > pr.first;  // lower must be less than max,
                                              // higher more than min
            }
            if (suitable)
                mq->select_ranges<double>(nm, vp);
        } else if (tp == TILEDB_INT32) {
            Rcpp::IntegerMatrix mm = lst[nm];
            Rcpp::IntegerMatrix::Column lo = mm.column(
                0);  // works as proxy for int and float types
            Rcpp::IntegerMatrix::Column hi = mm.column(
                1);  // works as proxy for int and float types
            std::vector<std::pair<int32_t, int32_t>> vp(mm.nrow());
            const std::pair<int32_t, int32_t> pr = dm->domain<int32_t>();
            for (int i = 0; i < mm.nrow(); i++) {
                vp[i] = std::make_pair(
                    std::max(lo[i], pr.first), std::min(hi[i], pr.second));
                tdbs::LOG_DEBUG(fmt::format("[apply_dim_ranges] Applying dim point {} on {} with {} - "
                    "{}",
                    i,
                    nm[i],
                    lo[i],
                    hi[i]));
                suitable = lo[i] < pr.second &&
                           hi[i] > pr.first;  // lower must be less than max,
                                              // higher more than min
            }
            if (suitable)
                mq->select_ranges<int32_t>(nm, vp);
        } else {
            Rcpp::stop(
                "Currently unsupported type: ", tiledb::impl::to_str(tp));
        }
        if (!suitable) {
            Rcpp::stop(
                "Unsuitable dim ranges on dimension '%s' with domain %s",
                nm,
                dm->domain_to_str());
        }
    }
}

// initialize arrow schema and array, respectively
Rcpp::XPtr<ArrowSchema> schema_setup_struct(
    Rcpp::XPtr<ArrowSchema> schxp, int64_t n_children) {
    ArrowSchema* schema = schxp.get();
    auto type = NANOARROW_TYPE_STRUCT;

    ArrowSchemaInit(schema);  // modified from ArrowSchemaInitFromType()
    int result = ArrowSchemaSetType(schema, type);
    if (result != NANOARROW_OK) {
        schema->release(schema);
        Rcpp::stop("Error setting struct schema");
    }

    // now adapted from ArrowSchemaAllocateChildren
    if (schema->children != NULL)
        Rcpp::stop("Error allocation as children not null");

    if (n_children > 0) {
        auto ptr = (struct ArrowSchema**)ArrowMalloc(
            n_children * sizeof(struct ArrowSchema*));
        Rcpp::XPtr<ArrowSchema*> schema_ptrxp = make_xptr(ptr, false);
        schema->children = schema_ptrxp.get();
        if (schema->children == NULL)
            Rcpp::stop("Failed to allocate ArrowSchema*");

        schema->n_children = n_children;
        memset(schema->children, 0, n_children * sizeof(struct ArrowSchema*));

        for (int64_t i = 0; i < n_children; i++) {
            schema->children[i] = schema_owning_xptr();
            if (schema->children[i] == NULL)
                Rcpp::stop("Error allocation schema child %ld", i);
            schema->children[i]->release = NULL;
        }
    }
    return schxp;
}

std::vector<int64_t> i64_from_rcpp_numeric(const Rcpp::NumericVector& input) {
    auto ndim = input.size();
    std::vector<int64_t> output(ndim);
    for (auto i = 0; i < ndim; i++) {
        output[i] = input[i];
    }

    return output;
}

// formerly stats.cpp

//' TileDB SOMA statistics
//'
//' These functions expose the TileDB Core functionality for performance
//'  measurements and statistics.
//'
//' - `tiledbsoma_stats_enable()`/`tiledbsoma_stats_disable()`: Enable and
//'    disable TileDB's internal statistics.
//' - `tiledbsoma_stats_reset()`: Reset all statistics to 0.
//' - `tiledbsoma_stats_dump()`: Dump all statistics to a JSON string.
//' - `tiledbsoma_stats_show()`: Print all statistics to the console.
//'
//' @name tiledbsoma_stats
//' @export
// [[Rcpp::export]]
void tiledbsoma_stats_enable() {
    tiledbsoma::stats::enable();
}

//' @rdname tiledbsoma_stats
//' @export
// [[Rcpp::export]]
void tiledbsoma_stats_disable() {
    tiledbsoma::stats::disable();
}

//' @rdname tiledbsoma_stats
//' @export
// [[Rcpp::export]]
void tiledbsoma_stats_reset() {
    tiledbsoma::stats::reset();
}

//' @rdname tiledbsoma_stats
//' @export
// [[Rcpp::export]]
std::string tiledbsoma_stats_dump() {
    return tiledbsoma::stats::dump();
}

// formerly version.cpp

//' libtiledbsoma version
//'
//' Returns a string with version information for libtiledbsoma and the linked
//' TileDB Embedded library. If argument `compact` is set to `TRUE`, a shorter
//' version of just the TileDB Embedded library version is returned.
//' @noRd
// [[Rcpp::export]]
std::string libtiledbsoma_version(
    const bool compact = false, const bool major_minor_only = false) {
    if (compact) {
        auto v = tiledbsoma::version::embedded_version_triple();
        std::ostringstream txt;
        if (major_minor_only) {
            txt << std::get<0>(v) << "." << std::get<1>(v);
        } else {
            txt << std::get<0>(v) << "." << std::get<1>(v) << "."
                << std::get<2>(v);
        }
        return txt.str();
    } else {
        return tiledbsoma::version::as_string();
    }
}

//' TileDB embedded version
//'
//' Gets the version of the TileDB Embedded library that is currently in use.
//' @noRd
// [[Rcpp::export]]
Rcpp::IntegerVector tiledb_embedded_version() {
    std::tuple<int, int, int>
        triple = tiledbsoma::version::embedded_version_triple();
    return Rcpp::IntegerVector::create(
        std::get<0>(triple), std::get<1>(triple), std::get<2>(triple));
}

// Also present in tiledb-r but only after 0.23.0 so this can be removed (and
// the call to it updated) once we hit a new tiledb-r release 0.24.0 (or 0.23.1)
//' @noRd
// [[Rcpp::export]]
size_t tiledb_datatype_max_value(const std::string& datatype) {
    if (datatype == "INT8")
        return std::numeric_limits<int8_t>::max();
    else if (datatype == "UINT8")
        return std::numeric_limits<uint8_t>::max();
    else if (datatype == "INT16")
        return std::numeric_limits<int16_t>::max();
    else if (datatype == "UINT16")
        return std::numeric_limits<uint16_t>::max();
    else if (datatype == "INT32")
        return std::numeric_limits<int32_t>::max();
    else if (datatype == "UINT32")
        return std::numeric_limits<uint32_t>::max();
    else if (datatype == "INT64")
        return std::numeric_limits<int64_t>::max();
    else if (datatype == "UINT64")
        return std::numeric_limits<uint64_t>::max();
    else
        Rcpp::stop("currently unsupported datatype (%s)", datatype);
}

// Make (optional) TimestampRange from (nullable, two-element)
// Rcpp::DatetimeVector
std::optional<tdbs::TimestampRange> makeTimestampRange(
    Rcpp::Nullable<Rcpp::DatetimeVector> tsvec) {
    // optional timestamp, defaults to 'none' aka std::nullopt
    std::optional<tdbs::TimestampRange> tsrng = std::nullopt;

    if (tsvec.isNotNull()) {
        // an Rcpp 'Nullable' is a decent compromise between adhering to SEXP
        // semantics and having 'optional' behaviour -- but when there is a
        // value we need to be explicit
        Rcpp::DatetimeVector vec(tsvec);  // vector of Rcpp::Datetime ie POSIXct
                                          // w/ (fract.) secs since epoch
        if (vec.size() == 1) {
            tsrng = std::make_pair<uint64_t>(
                0,
                static_cast<uint64_t>(
                    Rcpp::Datetime(vec[0]).getFractionalTimestamp() * 1000));
        } else if (vec.size() == 2) {
            tsrng = std::make_pair<uint64_t>(
                static_cast<uint64_t>(
                    Rcpp::Datetime(vec[0]).getFractionalTimestamp() * 1000),
                static_cast<uint64_t>(
                    Rcpp::Datetime(vec[1]).getFractionalTimestamp() * 1000));
        } else {
            Rcpp::stop("TimestampRange must be a one or two-element vector");
        }
    }

    return tsrng;
}

SEXP convert_domainish(const tdbs::ArrowTable& arrow_table) {
    ArrowArray* arrow_array = arrow_table.first.get();
    ArrowSchema* arrow_schema = arrow_table.second.get();

    auto schemaxp = nanoarrow_schema_owning_xptr();
    auto sch = nanoarrow_output_schema_from_xptr(schemaxp);
    exitIfError(
        ArrowSchemaInitFromType(sch, NANOARROW_TYPE_STRUCT), "Bad schema init");
    exitIfError(ArrowSchemaSetName(sch, ""), "Bad schema name");
    exitIfError(
        ArrowSchemaAllocateChildren(sch, arrow_schema->n_children),
        "Bad schema children alloc");

    if (arrow_array->n_children != arrow_schema->n_children) {
        Rcpp::stop(
            "schema/data column mismatch %d != %d\n",
            (int)arrow_array->n_children,
            (int)arrow_schema->n_children);
    }
    auto ncol = arrow_schema->n_children;

    auto arrayxp = nanoarrow_array_owning_xptr();
    auto arr = nanoarrow_output_array_from_xptr(arrayxp);
    exitIfError(
        ArrowArrayInitFromType(arr, NANOARROW_TYPE_STRUCT), "Bad array init");
    exitIfError(
        ArrowArrayAllocateChildren(arr, arrow_array->n_children),
        "Bad array children alloc");

    for (size_t i = 0; i < static_cast<size_t>(ncol); i++) {
        if (arrow_array->children[i]->n_buffers == 3) {
            // Arrow semantics: variable-length: buffers 0,1,2 are validity,
            // offsets, data
            std::vector<std::string>
                lohi = tiledbsoma::ArrowAdapter::get_array_string_column(
                    arrow_array->children[i], arrow_schema->children[i]);
            tdbs::LOG_INFO(fmt::format("[domainish] name {} format {} length {} lo {} hi {}",
                std::string(arrow_schema->children[i]->name),
                std::string(arrow_schema->children[i]->format),
                arrow_array->children[i]->length,
                lohi[0],
                lohi[1]));
        } else {
            // Arrow semantics: non-variable-length: buffers 0,1 are validity &
            // data
            tdbs::LOG_INFO(fmt::format("[domainish] name {} format {} length {}",
                std::string(arrow_schema->children[i]->name),
                std::string(arrow_schema->children[i]->format),
                arrow_array->children[i]->length));
        }

        ArrowArrayMove(arrow_array->children[i], arr->children[i]);
        ArrowSchemaMove(arrow_schema->children[i], sch->children[i]);
    }

    // Nanoarrow special: stick schema into xptr tag to return single SEXP
    array_xptr_set_schema(arrayxp, schemaxp);  // embed schema in array

    return arrayxp;
}

static std::map<std::string, std::string> _type_name_remap = {
    {"int8", "c"},
    {"int16", "s"},
    {"int32", "i"},
    {"int64", "l"},
    {"uint8", "C"},
    {"uint16", "S"},
    {"uint32", "I"},
    {"uint64", "L"},
    {"utf8", "u"},
    {"large_utf8", "U"},
    {"bool", "b"},
    {"float", "f"},
    {"double", "g"}};

std::string remap_arrow_type_code_r_to_c(std::string input) {
    auto it = _type_name_remap.find(input);
    if (it == _type_name_remap.end()) {
        return input;
    } else {
        return it->second;
    }
}
