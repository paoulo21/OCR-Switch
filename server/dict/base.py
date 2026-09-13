from abc import ABC, abstractmethod
from typing import List, Optional
from pydantic import BaseModel

class DictDefinition(BaseModel):
    word: str
    reading: Optional[str] = None
    definitions: List[str] = []
    pitch: List[str] = []
    pos: List[str] = []
    dict_name: str = "Dictionary"

class TokenMatch(BaseModel):
    word: str
    reading: Optional[str] = None
    definitions: List[str] = []
    pitch: List[str] = []
    char_range: List[int] = [0, 0]  # [start_char_idx, end_char_idx]

class BaseDictEngine(ABC):
    @abstractmethod
    def lookup_sentence(self, sentence: str) -> List[TokenMatch]:
        """Parse sentence, deinflect and return matching dictionary tokens with char offsets."""
        pass

    @abstractmethod
    def query_term(self, term: str) -> List[DictDefinition]:
        """Look up a specific term and return definitions."""
        pass
