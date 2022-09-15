import os
from abc import ABC, abstractmethod
from typing import Optional, Sequence, Union

import tiledb

import tiledbsoma

from . import util
from .soma_metadata_mapping import SOMAMetadataMapping
from .tiledb_platform_config import TileDBPlatformConfig


class TileDBObject(ABC):
    """
    Base class for ``TileDBArray`` and ``SOMACollection``.

    Manages tiledb_platform_config, context, etc. which are common to both.
    """

    _uri: str
    _name: str
    _nested_name: str
    _tiledb_platform_config: TileDBPlatformConfig
    metadata: SOMAMetadataMapping

    def __init__(
        self,
        # All objects:
        uri: str,
        name: Optional[str] = None,
        *,
        # Non-top-level objects can have a parent to propgate context, depth, etc.
        parent: Optional["tiledbsoma.SOMACollection"] = None,
        # Top-level objects should specify these:
        tiledb_platform_config: Optional[TileDBPlatformConfig] = None,
        ctx: Optional[tiledb.Ctx] = None,
    ):
        """
        Initialization-handling shared between ``TileDBArray`` and ``SOMACollection``.  Specify ``tiledb_platform_config`` and ``ctx`` for the top-level object; omit them and specify parent for non-top-level objects. Note that the parent reference is solely for propagating options, ctx, display depth, etc.
        """
        self._uri = uri
        if name is None:
            self._name = os.path.basename(uri)
        else:
            self._name = name

        if parent is None:
            self._ctx = ctx
            self._indent = ""
            self._nested_name = self._name
        else:
            tiledb_platform_config = parent._tiledb_platform_config
            self._ctx = parent._ctx
            self._indent = parent._indent + "  "
            self._nested_name = parent._nested_name + "/" + self._name

        self._tiledb_platform_config = tiledb_platform_config or TileDBPlatformConfig()
        # Null ctx is OK if that's what they wanted (e.g. not doing any TileDB-Cloud ops).

        self.metadata = SOMAMetadataMapping(self)

    def __repr__(self) -> str:
        """
        Fallback string display. Will be overridden by any interesting subclasses.
        """
        return f"name={self._name},uri={self._uri}"

    def _repr_aux(self) -> Sequence[str]:
        raise Exception("Must be overridden by inherting classes.")

    def get_name(self) -> str:
        return self._name

    def get_uri(self) -> str:
        return self._uri

    def get_type(self) -> str:
        return type(self).__name__

    def exists(self) -> bool:
        """
        Returns true if the object exists and has the desired class name.

        This might be in case an object has not yet been populated, or, if a containing object has been populated but doesn't have a particular member (e.g. not all ``SOMAMeasurement`` objects have a ``varp``).

        For ``tiledb://`` URIs this is a REST-server request which we'd like to cache.  However, remove-and-replace use-cases are possible and common in notebooks and it turns out caching the existence-check isn't a robust approach.
        """

        # Pre-checking if the group exists by calling tiledb.object_type is simple, however, for
        # tiledb-cloud URIs that occurs a penalty of two HTTP requests to the REST server, even
        # before a third, successful HTTP request for group-open.  Instead, we directly attempt the
        # group-open request, checking for an exception.
        try:
            return self._get_object_type_from_metadata() == self.get_type()
        except tiledb.cc.TileDBError:
            return False

    @abstractmethod
    def _tiledb_open(self, mode: str = "r") -> Union[tiledb.Array, tiledb.Group]:
        """Open the underlying TileDB array or Group"""

    def _common_create(self) -> None:
        """
        Utility method for various constructors.
        """
        self._set_object_type_metadata()

    def _set_object_type_metadata(self) -> None:
        """
        This helps nested-structure traversals (especially those that start at the SOMACollection level) confidently navigate with a minimum of introspection on group contents.
        """
        # TODO: make a multi-set in SOMAMetadataMapping that would above a double-open there.
        with self._tiledb_open("w") as obj:
            obj.meta.update(
                {
                    util.SOMA_OBJECT_TYPE_METADATA_KEY: self.get_type(),
                    util.SOMA_ENCODING_VERSION_METADATA_KEY: util.SOMA_ENCODING_VERSION,
                }
            )

    def _get_object_type_from_metadata(self) -> str:
        """
        Returns the class name associated with the group/array.
        """
        # mypy says:
        # error: Returning Any from function declared to return "str"  [no-any-return]
        return self.metadata.get(util.SOMA_OBJECT_TYPE_METADATA_KEY)  # type: ignore