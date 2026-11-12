"""Numbers for the essay from the bisection (results/swmm6/db4634a/bisect.jsonl.changes.json), the moves
against EPA 5.2.4 (moves.json) and the conduit-loss control (losses_40f439a.json)."""
import collections

def per_commit(changes, mv):
    """{commit: {...}} in commit order, plus, per model, the commits that changed it."""
    verdict = {}
    for e in mv['engines'].values():
        for x in e['models']: verdict[x['inp']] = x
    order, C = [], {}
    by_model = collections.defaultdict(set)
    for ch in changes['changes']:
        c = ch['after']
        if c not in C:
            C[c] = dict(subject=ch['subject'], L=[], N=[], untestable=ch['untestable_between']); order.append(c)
        C[c][ch['engine']] += ch['models']
        for m in ch['models']: by_model[m].add(c)
    for c in order:
        ms = sorted(set(C[c]['L']) | set(C[c]['N']))
        C[c]['models'] = ms
        C[c]['only_here'] = [m for m in ms if by_model[m] == {c}]
        C[c]['verdicts'] = collections.Counter(verdict[m]['verdict'] for m in ms if m in verdict)
    return order, C, by_model, verdict

def bisect_numbers(changes, mv, losses, words, n_listed, n_src_commits):
    order, C, by_model, verdict = per_commit(changes, mv)
    N = {}
    N['UP_BISECT_N'] = n_listed
    N['UP_BISECT_BUILT'] = changes['built']
    N['UP_SRC_COMMITS'] = n_src_commits
    c1 = max(order, key=lambda c: len(C[c]['models']))
    assert c1 == '40f439a', c1
    k1 = C[c1]
    N['UP_C1_N'] = len(k1['models'])
    v = k1['verdicts']
    N['UP_C1_FURTHER'] = v.get('further', 0); N['UP_C1_CLOSER'] = words(v.get('closer', 0))
    N['UP_C1_SAME'] = v.get('about same', 0)
    fur = [verdict[m] for m in k1['models'] if verdict[m]['verdict'] == 'further']
    N['UP_C1_FROM_EXACT'] = sum(1 for x in fur if x['exact_before'])
    N['UP_C1_MAXF'] = f"{max(x['after'] for x in fur):.0f}"
    dw = losses['dynwave']
    assert dw.get('without losses, moved', 0) == 0
    N['UP_C1_NOLOSS'] = f"{dw['without losses, unchanged']:,}"
    N['UP_C1_LOSSY'] = f"{dw['with losses, moved'] + dw['with losses, unchanged']:,}"
    assert dw['with losses, moved'] == len(k1['models'])
    kb = losses['moved_by_kind']
    N['UP_C1_PUMPS'] = kb.get('with pumps', 0); N['UP_C1_CTL'] = kb.get('with control rules', 0)
    rc = losses['routing_continuity_legacy']
    N['UP_C1_CONT_SAME'] = rc['same_within_0.005']
    worst = min(rc['rows'], key=lambda r: r[1])          # most negative continuity before
    wv = verdict[worst[0]]
    N['UP_C1_WORST_MODEL'] = '<code>' + worst[0].split('/')[-1] + '</code>'
    N['UP_C1_WORST_B'] = f"{worst[1]:,.1f}".replace('-', '&minus;'); N['UP_C1_WORST_C'] = f"{worst[2]:,.1f}".replace('-', '&minus;')
    N['UP_C1_WORST_D0'] = f"{wv['before']:.2f}"; N['UP_C1_WORST_D1'] = f"{wv['after']:.0f}"
    return N, order, C, by_model, verdict
