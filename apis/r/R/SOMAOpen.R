#' @title Open a SOMA Object
#' @description Utility function to open the corresponding SOMA Object given a URI, (lifecycle: maturing)
#' @param mode One of `"READ"` or `"WRITE"`
#' @param uri URI for the TileDB object
#' @param platform_config Optional platform configuration
#' @param tiledbsoma_ctx Optional SOMATileDBContext
#' @param tiledb_timestamp Optional Datetime (POSIXct) with TileDB timestamp. For SOMACollections,
#'        all accessed members inherit the collection opening timestamp, and in READ mode the
#'        collection timestamp defaults to the time of opening.
#'
#' @export
#'
SOMAOpen <- function(
  uri,
  mode = "READ",
  platform_config = NULL,
  tiledbsoma_ctx = NULL,
  tiledb_timestamp = NULL
) {
  # As an alternative we could rely tiledb-r and its tiledb_object_type but
  # this would require instantiating a ctx object first. It is a possible
  # refinement if and when we decide to hold an array or group pointer.
  # For now, first attempt to instantiate a TileDBArray to take advantage of
  # its handling of the config and ctx object as well as the caching
  obj <- tryCatch(
    expr = {
      arr <- TileDBArray$new(uri,
        platform_config = platform_config,
        tiledbsoma_ctx = tiledbsoma_ctx,
        tiledb_timestamp = tiledb_timestamp,
        internal_use_only = "allowed_use"
      )
      arr$open(mode = "READ", internal_use_only = "allowed_use")
      obj <- arr$get_metadata("soma_object_type")
      arr$close()
      obj
    },
    error = function(...) NULL,
    finally = function(...) NULL
  )

  # In case of an error try again as TileDBGroup
  if (is.null(obj)) {
    obj <- tryCatch(
      expr = {
        grp <- TileDBGroup$new(uri,
          platform_config = platform_config,
          tiledbsoma_ctx = tiledbsoma_ctx,
          tiledb_timestamp = tiledb_timestamp,
          internal_use_only = "allowed_use"
        )
        grp$open(mode = "READ", internal_use_only = "allowed_use")
        obj <- grp$get_metadata("soma_object_type")
        grp$close()
        obj
      },
      error = function(...) NULL,
      finally = function(...) NULL
    )
  }

  # If this also errored no other
  if (is.null(obj)) {
    stop("URI '", uri, "' is not a TileDB SOMA object.", call. = FALSE)
  }

  switch(
    EXPR = obj,
    SOMACollection = SOMACollectionOpen(
      uri,
      mode = mode,
      platform_config = platform_config,
      tiledbsoma_ctx = tiledbsoma_ctx,
      tiledb_timestamp = tiledb_timestamp
    ),
    SOMADataFrame = SOMADataFrameOpen(
      uri,
      mode = mode,
      platform_config = platform_config,
      tiledbsoma_ctx = tiledbsoma_ctx,
      tiledb_timestamp = tiledb_timestamp
    ),
    SOMADenseNDArray = SOMADenseNDArrayOpen(
      uri,
      mode = mode,
      platform_config = platform_config,
      tiledbsoma_ctx = tiledbsoma_ctx,
      tiledb_timestamp = tiledb_timestamp
    ),
    SOMASparseNDArray = SOMASparseNDArrayOpen(
      uri,
      mode = mode,
      platform_config = platform_config,
      tiledbsoma_ctx = tiledbsoma_ctx,
      tiledb_timestamp = tiledb_timestamp
    ),
    SOMAExperiment = SOMAExperimentOpen(
      uri,
      mode = mode,
      platform_config = platform_config,
      tiledbsoma_ctx = tiledbsoma_ctx,
      tiledb_timestamp = tiledb_timestamp
    ),
    SOMAMeasurement = SOMAMeasurementOpen(
      uri,
      mode = mode,
      platform_config = platform_config,
      tiledbsoma_ctx = tiledbsoma_ctx,
      tiledb_timestamp = tiledb_timestamp
    ),
    stop(sprintf("No support for type '%s'", obj), call. = FALSE)
  )
}
