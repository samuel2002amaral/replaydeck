#!/usr/bin/env python3
"""Lista os textos traduzíveis do código (tl("..."), tlc("..."), N_("...")).

Uso:
  tools/i18n-extract.py                 -> escreve i18n/keys.txt (um texto por linha, escapes \\n)
  tools/i18n-extract.py --missing en    -> mostra os textos que faltam em data/lang/en.json
"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CALL = re.compile(r'\b(?:tl|tlc|N_)\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)')
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')


def unescape(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s):
            n = s[i + 1]
            out.append({'n': '\n', 't': '\t', '"': '"', '\\': '\\', "'": "'"}.get(n, '\\' + n))
            i += 2
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def keys():
    found = []
    for d, _, files in os.walk(os.path.join(ROOT, 'src')):
        for f in sorted(files):
            if not f.endswith('.cpp'):
                continue
            src = open(os.path.join(d, f), encoding='utf-8').read()
            for m in CALL.finditer(src):
                k = unescape(''.join(LIT.findall(m.group(1))))
                if k and k not in found:
                    found.append(k)
    return found


if __name__ == '__main__':
    ks = keys()
    if len(sys.argv) > 2 and sys.argv[1] == '--missing':
        have = json.load(open(os.path.join(ROOT, 'data', 'lang', sys.argv[2] + '.json'), encoding='utf-8'))
        miss = [k for k in ks if k not in have]
        for k in miss:
            print(json.dumps(k, ensure_ascii=False))
        print(f'{len(miss)} faltando de {len(ks)}', file=sys.stderr)
        sys.exit(1 if miss else 0)
    os.makedirs(os.path.join(ROOT, 'i18n'), exist_ok=True)
    with open(os.path.join(ROOT, 'i18n', 'keys.txt'), 'w', encoding='utf-8') as fh:
        for k in ks:
            fh.write(k.replace('\\', '\\\\').replace('\n', '\\n') + '\n')
    print(len(ks), 'textos', sum(len(k) for k in ks), 'caracteres')
