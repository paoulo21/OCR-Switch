from typing import List, Tuple

# Common Japanese inflections: (inflected_suffix, base_suffix, rule_name)
DEINFLECTION_RULES: List[Tuple[str, str, str]] = [
    # Past / Perfective (た/だ)
    ("た", "る", "ichidan past"),
    ("た", "る", "godan-ru past"),
    ("いた", "く", "godan-ku past"),
    ("いだ", "ぐ", "godan-gu past"),
    ("した", "す", "godan-su past"),
    ("った", "う", "godan-u past"),
    ("った", "つ", "godan-tsu past"),
    ("った", "る", "godan-ru past"),
    ("んだ", "ぬ", "godan-nu past"),
    ("んだ", "ぶ", "godan-bu past"),
    ("んだ", "む", "godan-mu past"),
    ("行った", "行く", "iku past"),
    ("した", "する", "suru past"),
    ("きた", "くる", "kuru past"),
    ("来た", "来る", "kuru past"),

    # Te-form (て/で)
    ("て", "る", "ichidan te"),
    ("いて", "く", "godan-ku te"),
    ("いで", "ぐ", "godan-gu te"),
    ("して", "す", "godan-su te"),
    ("って", "う", "godan-u te"),
    ("って", "つ", "godan-tsu te"),
    ("って", "る", "godan-ru te"),
    ("んで", "ぬ", "godan-nu te"),
    ("んで", "ぶ", "godan-bu te"),
    ("んで", "む", "godan-mu te"),
    ("行って", "行く", "iku te"),
    ("して", "する", "suru te"),
    ("きて", "くる", "kuru te"),
    ("来て", "来る", "kuru te"),

    # Polite Masu-form (ます, ました, ません, ませんでした)
    ("ます", "る", "ichidan masu"),
    ("ます", "う", "godan-u masu"),
    ("きます", "く", "godan-ku masu"),
    ("ぎます", "ぐ", "godan-gu masu"),
    ("します", "す", "godan-su masu"),
    ("ちます", "つ", "godan-tsu masu"),
    ("にます", "ぬ", "godan-nu masu"),
    ("びます", "ぶ", "godan-bu masu"),
    ("みます", "む", "godan-mu masu"),
    ("ります", "る", "godan-ru masu"),
    ("します", "する", "suru masu"),
    ("きます", "くる", "kuru masu"),
    ("来ます", "来る", "kuru masu"),

    ("ました", "る", "ichidan past masu"),
    ("いました", "う", "godan-u past masu"),
    ("きました", "く", "godan-ku past masu"),
    ("ぎました", "ぐ", "godan-gu past masu"),
    ("しました", "す", "godan-su past masu"),
    ("ちました", "つ", "godan-tsu past masu"),
    ("にました", "ぬ", "godan-nu past masu"),
    ("びました", "ぶ", "godan-bu past masu"),
    ("みました", "む", "godan-mu past masu"),
    ("りました", "る", "godan-ru past masu"),
    ("しました", "する", "suru past masu"),
    ("きました", "くる", "kuru past masu"),
    ("来ました", "来る", "kuru past masu"),

    ("ません", "る", "ichidan negative masu"),
    ("いません", "う", "godan-u negative masu"),
    ("きません", "く", "godan-ku negative masu"),
    ("ぎません", "ぐ", "godan-gu negative masu"),
    ("しません", "す", "godan-su negative masu"),
    ("ちません", "つ", "godan-tsu negative masu"),
    ("にません", "ぬ", "godan-nu negative masu"),
    ("びません", "ぶ", "godan-bu negative masu"),
    ("みません", "む", "godan-mu negative masu"),
    ("りません", "る", "godan-ru negative masu"),
    ("しません", "する", "suru negative masu"),
    ("きません", "くる", "kuru negative masu"),
    ("来ません", "来る", "kuru negative masu"),

    # Negative (ない, なかった)
    ("ない", "る", "ichidan negative"),
    ("わない", "う", "godan-u negative"),
    ("かない", "く", "godan-ku negative"),
    ("がない", "ぐ", "godan-gu negative"),
    ("さない", "す", "godan-su negative"),
    ("たない", "つ", "godan-tsu negative"),
    ("なない", "ぬ", "godan-nu negative"),
    ("ばない", "ぶ", "godan-bu negative"),
    ("まない", "む", "godan-mu negative"),
    ("らない", "る", "godan-ru negative"),
    ("しない", "する", "suru negative"),
    ("こない", "くる", "kuru negative"),
    ("来ない", "来る", "kuru negative"),

    # Potential / Passive (られる, える, れる)
    ("られる", "る", "ichidan potential/passive"),
    ("える", "う", "godan-u potential"),
    ("ける", "く", "godan-ku potential"),
    ("げる", "ぐ", "godan-gu potential"),
    ("せる", "す", "godan-su potential"),
    ("てる", "つ", "godan-tsu potential"),
    ("ねる", "ぬ", "godan-nu potential"),
    ("べる", "ぶ", "godan-bu potential"),
    ("める", "む", "godan-mu potential"),
    ("れる", "る", "godan-ru potential"),
    ("できる", "する", "suru potential"),
    ("出来る", "する", "suru potential"),

    # Volitional (よう, おう)
    ("よう", "る", "ichidan volitional"),
    ("おう", "う", "godan-u volitional"),
    ("こう", "く", "godan-ku volitional"),
    ("ごう", "ぐ", "godan-gu volitional"),
    ("そう", "す", "godan-su volitional"),
    ("とう", "つ", "godan-tsu volitional"),
    ("のう", "ぬ", "godan-nu volitional"),
    ("ぼう", "ぶ", "godan-bu volitional"),
    ("もう", "む", "godan-mu volitional"),
    ("ろう", "る", "godan-ru volitional"),
    ("しよう", "する", "suru volitional"),

    # Causative (させる, す)
    ("させる", "る", "ichidan causative"),
    ("わせる", "う", "godan-u causative"),
    ("かせる", "く", "godan-ku causative"),
    ("がせる", "ぐ", "godan-gu causative"),
    ("させる", "す", "godan-su causative"),
    ("たせる", "つ", "godan-tsu causative"),
    ("なせる", "ぬ", "godan-nu causative"),
    ("ばせる", "ぶ", "godan-bu causative"),
    ("ませる", "む", "godan-mu causative"),
    ("らせる", "る", "godan-ru causative"),

    # Imperative (ろ, え)
    ("ろ", "る", "ichidan imperative"),
    ("え", "う", "godan-u imperative"),
    ("け", "く", "godan-ku imperative"),
    ("げ", "ぐ", "godan-gu imperative"),
    ("せ", "す", "godan-su imperative"),
    ("て", "つ", "godan-tsu imperative"),
    ("ね", "ぬ", "godan-nu imperative"),
    ("べ", "ぶ", "godan-bu imperative"),
    ("め", "む", "godan-mu imperative"),
    ("れ", "る", "godan-ru imperative"),
    ("しろ", "する", "suru imperative"),
    ("こい", "くる", "kuru imperative"),
    ("来い", "来る", "kuru imperative"),

    # I-Adjectives (かった, く, くない, くなかった, ければ)
    ("かった", "い", "i-adj past"),
    ("くない", "い", "i-adj negative"),
    ("くなかった", "い", "i-adj past negative"),
    ("くて", "い", "i-adj te"),
    ("ければ", "い", "i-adj conditional"),
]

def deinflect(word: str) -> List[str]:
    """Return a list of potential base (dictionary) forms for the given Japanese word."""
    candidates = [word]
    seen = {word}

    for suffix, replacement, _ in DEINFLECTION_RULES:
        if word.endswith(suffix):
            base = word[:-len(suffix)] + replacement
            if base and base not in seen:
                candidates.append(base)
                seen.add(base)

    return candidates
