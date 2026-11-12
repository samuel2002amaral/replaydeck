#!/usr/bin/env python3
"""Monta data/lang/<código>.json a partir de i18n/<código>.txt (linhas "N|tradução",
na mesma ordem de i18n/keys.txt) e confere %1/%2, quebras de linha e tags.

Uso: tools/i18n-build.py en es fr ...   (sem argumentos: todos os i18n/*.txt)
"""
import glob, json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def unesc(s):
    return s.replace('\\n', '\n').replace('\\\\', '\\')


def marks(s):
    return (sorted(re.findall(r'%\d', s)), s.count('\n'), sorted(re.findall(r'</?[a-z]+', s)))


keys = [unesc(l.rstrip('\n')) for l in open(os.path.join(ROOT, 'i18n', 'keys.txt'), encoding='utf-8')]
codes = sys.argv[1:] or sorted(os.path.basename(p)[:-4] for p in glob.glob(os.path.join(ROOT, 'i18n', '*.txt'))
                               if not p.endswith('keys.txt'))
bad = 0
for code in codes:
    lines = [l.rstrip('\n') for l in open(os.path.join(ROOT, 'i18n', code + '.txt'), encoding='utf-8') if l.strip()]
    out = {}
    seen = set()
    for l in lines:
        n, _, t = l.partition('|')
        i = int(n) - 1
        if i in seen or not 0 <= i < len(keys):
            print(f'{code}: linha {n} repetida ou fora da lista'); bad += 1; continue
        seen.add(i)
        t = unesc(t)
        k = keys[i]
        if marks(k) != marks(t):
            print(f'{code}:{n}: marcadores diferentes\n  pt: {k!r}\n  {code}: {t!r}'); bad += 1
        out[k] = t
    miss = [str(i + 1) for i in range(len(keys)) if i not in seen]
    if miss:
        print(f'{code}: faltam as linhas {", ".join(miss[:30])}'); bad += 1
    os.makedirs(os.path.join(ROOT, 'data', 'lang'), exist_ok=True)
    with open(os.path.join(ROOT, 'data', 'lang', code + '.json'), 'w', encoding='utf-8') as fh:
        json.dump(out, fh, ensure_ascii=False, indent=0, sort_keys=False)
    print(f'{code}: {len(out)} textos')
sys.exit(1 if bad else 0)
