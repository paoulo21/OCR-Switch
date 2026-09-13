import logging
from server.dict.base import BaseDictEngine
from server.dict.hoshidicts_cli import HoshidictsCLIEngine
from server.dict.yomitan_py import YomitanPyEngine

logger = logging.getLogger("switch-ocr.dict.manager")

_active_dict_engine: BaseDictEngine = None

def get_dict_engine() -> BaseDictEngine:
    global _active_dict_engine
    if _active_dict_engine is not None:
        return _active_dict_engine

    cli = HoshidictsCLIEngine()
    if cli.is_available():
        logger.info("Using Hoshidicts C++ CLI as primary dictionary engine.")
        _active_dict_engine = cli
        return _active_dict_engine

    logger.info("Using Yomitan Python engine as primary dictionary engine.")
    _active_dict_engine = YomitanPyEngine()
    return _active_dict_engine
