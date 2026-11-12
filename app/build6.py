"""Numbers and figures for the SWMM6 comparison section, from results/swmm6/ in the repository."""
import os, json, math, collections
import figures6 as F6

def _J(p): return json.load(open(p))

_W = ['no', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten', 'eleven', 'twelve',
      'thirteen', 'fourteen', 'fifteen', 'sixteen', 'seventeen', 'eighteen', 'nineteen', 'twenty']
def _words(n, cap=False):
    w = _W[n] if 0 <= n < len(_W) else f'{n:,}'
    return w[0].upper() + w[1:] if cap and w[0].isalpha() else w

def ratios(bench, b, key='cpu'):
    R = bench['results']; per = []
    for m in R:
        pr = [y[key] / x[key] for x, y in zip(R[m]['ref'], R[m][b])]
        per.append(sorted(pr)[len(pr) // 2])
    return math.exp(sum(math.log(v) for v in per) / len(per)), per


def _timeouts_text(refused, sm):
    """The new engine's time-outs on models EPA runs, and its stops without an ERROR code among models EPA and the legacy engine both run."""
    tos = [t for t in sm.get('timeouts_new', []) if t['epa_output']]
    crashes = [m for m, e, rc in refused if not e and rc != 'timeout']
    words = {1: 'One', 2: 'Two', 3: 'Three', 4: 'Four'}
    out = ''
    if crashes:
        kinds = []
        if any('IO-41' in m for m in crashes): kinds.append('a code-review deck with a malformed exponent in an expression, which aborts')
        if any('CRASH-09' in m for m in crashes): kinds.append('a code-review deck whose snow removal names no destination, which the new engine now refuses with a message of its own')
        other = len(crashes) - len(kinds)
        if other > 0: kinds.append(f'{other} more')
        out += f"{words.get(len(crashes), len(crashes))} stopped without an ERROR code: " + '; '.join(kinds) + '. '
    if tos:
        io = [t for t in tos if 'IO-02' in t['inp']]
        ky = [t for t in tos if '1000year' in t['inp']]
        rest = [t for t in tos if t not in io and t not in ky]
        out += f"{words.get(len(tos), len(tos))} more ran past the five-minute limit, with two models running at once. "
        if io:
            out += ('One is a code-review deck whose start time is NaN: EPA and the legacy engine finish it at once with no '
                    'reporting periods, and the new engine had written close to a gigabyte of output when it was stopped. ')
        if len(ky) == 1:
            out += f"Another is a 1,000-year simulation that EPA finishes in {ky[0]['epa_s']:.0f} seconds and the legacy engine in {ky[0]['legacy_s']:.0f}. "
        elif ky:
            out += (f"{words.get(len(ky), len(ky))} are 1,000-year simulations that EPA finishes in "
                    + ' and '.join(f"{t['epa_s']:.0f}" for t in ky) + ' seconds and the legacy engine in '
                    + ' and '.join(f"{t['legacy_s']:.0f}" for t in ky) + '. ')
        out += ' '.join(t['inp'] for t in rest)
    return out.strip()

# What each commit that changed a result does, in a phrase; written from its message and diff.
COMMIT_WHAT = {
    '40f439a': 'limits conduit evaporation and seepage to the water at both end nodes; per-barrel loss cap',
    'e5f96a4': 'adds an optional storage area to junctions; its finiteness check also rejects an infinite junction depth',
    '7bcb4f4': 'fixes defects from the alpha.4 engine review; validating snow fractions, it refuses snow removal that names no destination',
    'a47a3dc': 'reads the covered flag on rain barrels, as the legacy engine already did',
    'fe8f84f': 'returns only the rain on a covered rain barrel itself',
    'ab1d439': 'mixes LID return flow into the ponded water whose quality it carries',
    '79e8e34': 'applies the permeable pavement regeneration degree, and rejects one outside 0 to 1',
}

def rest_text(order, CC, by_model, verdict, words):
    """The commits other than 40f439a, as a table and a paragraph, from the bisection report."""
    missing = [c for c in order if c not in COMMIT_WHAT]
    assert not missing, 'describe these commits in COMMIT_WHAT: ' + ', '.join(missing)
    N = {}
    rows = []
    def tally(c):
        v = CC[c]['verdicts']; parts = []
        for k, lab in (('further', 'further from EPA'), ('closer', 'closer'), ('about same', 'same distance'),
                       ('now refused', 'now refused'), ('no EPA', 'no EPA result')):
            if v.get(k): parts.append(f"{v[k]} {lab}")
        return ', '.join(parts)
    for c in order:
        k = CC[c]
        rows.append([f"<code>{c}</code>", k['subject'], COMMIT_WHAT[c], f"{len(k['L'])} / {len(k['N'])}", tally(c)])
    N['UP_TABLE_COMMITS'] = (['Commit', 'Message', 'What it changes', 'Models changed, legacy / new', 'Against EPA 5.2.4'], rows)
    others = [c for c in order if c != '40f439a']
    assert set(others) == {'e5f96a4', '7bcb4f4', '79e8e34', 'a47a3dc', 'fe8f84f', 'ab1d439'}, others
    N['UP_REST_N'] = words(len(others))
    lid = ('a47a3dc', 'fe8f84f', 'ab1d439')
    N['UP_LID_N'] = words(len(set().union(*[set(CC[c]['models']) for c in lid])))
    for c in lid:
        v = CC[c]['verdicts']; assert not v.get('further') and not v.get('closer'), (c, v)
    twice = sorted(m for m, cs in by_model.items() if len(cs) > 1)
    N['UP_TWICE_TXT'] = (f"{words(len(twice), cap=True)} models changed at two commits each. " if twice else '')
    N['UP_COMMITS_TXT'] = (f"{len(CC['40f439a']['models'])} of them through one commit that limits "
                           "conduit evaporation and seepage to the water their end nodes hold.")
    N['UP_WHO_SHORT'] = (f"one commit, a fix that limits pipe evaporation and seepage to the water at the pipe's ends, "
                         f"moved {len(CC['40f439a']['models'])} of them.")
    N['UP_BISECT_LIMIT'] = ("The commits were found by bisection on the models whose results changed and that EPA 5.2.4 "
                            "runs, using a build without link-time optimization that reproduced the gate's builds at both "
                            "ends; a model is credited to the first commit at which its output bytes changed, and the "
                            "SWMM6-only test decks were not bisected.")
    return N

def build(repo, base_num, links=None):
    D = f'{repo}/results/swmm6'
    U = f'{D}/db4634a'                          # the rerun at the newer commit
    st, tr = _J(f'{D}/static.json'), _J(f'{D}/trace_ex05.json')
    sA = _J(f'{D}/summary.json')                # c40580c as first built (FP contraction on)
    sB, sC = _J(f'{U}/summary_B.json'), _J(f'{U}/summary_C.json')
    sm = sC                                     # the gate as it stands now
    growth = _J(f'{D}/trace_ex05_growth.json')
    att = _J(f'{D}/attribution.json') if os.path.exists(f'{D}/attribution.json') else None
    def _alias(b):
        # the rerun names SWMM6's builds A, B and C; the text speaks of the current one, C
        for m in b['results'].values():
            if 'swmm6_new_C' in m: m['swmm6_new'] = m['swmm6_new_C']
            if 'swmm6_legacy_C' in m: m['swmm6_legacy'] = m['swmm6_legacy_C']
        return b
    bench = (_alias(_J(f'{U}/bench6u_small.json')) if os.path.exists(f'{U}/bench6u_small.json')
             else _J(f'{D}/bench_swmm6.json'))       # placeholder until the retiming has run
    L, C, S = st['lines'], st['counts'], st['subsystems']
    def ok(p):
        s = sm['status'][p]; return s.get('identical values', 0), s.get('identical values', 0) + s.get('differs', 0)
    N = {}
    N['S6_DATE'] = '12 November 2026'
    N['S6_COMMIT'] = '<code>db4634a</code>'; N['S6_COMMIT_TXT'] = 'db4634a'
    N['S6_STATIC_DATE'] = '28 October 2026' if st['swmm6_date'] == '2026-10-28' else st['swmm6_date']
    N['S6_STATIC_COMMIT'] = '<code>' + st['swmm6_commit'][:7] + '</code>'
    N['S6_VERSIONS'] = 'new engine at 6.0.0-alpha.4 and the legacy engine at 5.3.0-beta.3'
    N['EPA_LINES'] = f"{L['epa_solver']:,}"; N['CP_LINES'] = f"{L['cplus_engine']:,}"
    N['S6_LINES'] = f"{L['swmm6_engine']:,}"; N['S6_LINES_2D'] = f"{L['swmm6_2d']:,}"
    N['EPA_API'] = C['epa_api_functions']; N['CP_HANDLE'] = C['cplus_handle_functions']; N['S6_API'] = f"{C['api_functions']:,}"
    N['S6_THROW'] = C['throw']; N['S6_OMP'] = C['omp_pragmas']; N['S6_PARITY'] = C['parity_notes']
    N['S6_X_EPA'] = f"{L['swmm6_engine'] / L['epa_solver']:.1f}"; N['S6_TESTS_X'] = f"{L['swmm6_tests'] / L['epa_solver']:.1f}"
    N['S6_DW_EPA'] = f"{S['dynamic wave'][0]:,}"; N['S6_DW_S6'] = f"{S['dynamic wave'][2]:,}"
    N['S6_XS_EPA'] = f"{S['cross sections'][0]:,}"; N['S6_XS_S6'] = f"{S['cross sections'][2]:,}"
    N['EPA_NODE_BYTES'] = st.get('epa_sizeof', {}).get('TNode', 256); N['EPA_LINK_BYTES'] = st.get('epa_sizeof', {}).get('TLink', 368)
    lv = st['legacy_vs_524']
    N['S6_LEG_DWFLOW'] = f"{lv['dwflow.c']['tokens_changed']:,}"; N['S6_LEG_DWFLOW_N'] = f"{lv['dwflow.c']['tokens_524']:,}"
    N['S6_LEG_ROUTING'] = f"{lv['routing.c']['tokens_changed']:,}"; N['S6_LEG_ROUTING_N'] = f"{lv['routing.c']['tokens_524']:,}"
    la = st.get('legacy_vs_524_all', {})
    N['S6_LEG_FILES'] = sum(1 for v in la.values() if v.get('tokens_changed')); N['S6_LEG_FILES_N'] = len(la)
    rc = st['regression_cases']
    N['S6_REG_N'] = len(rc)
    N['S6_REG_LIST'] = ', '.join(c[0] for c in rc[:-1]) + ' and ' + rc[-1][0]
    N['S6_REG_TOL'] = f"a difference of {rc[0][2]} in project units or {float(rc[0][3]) * 100:g} percent"
    pc = st['parity_corpus']; N['S6_CORPUS_DECKS'] = pc['decks']; N['S6_CORPUS_FV'] = pc['fv']
    N['CORPUS_N_SHORT'] = f"{base_num['FULL_SAME']} identical of {base_num['FULL_DONE']} run"
    # gate
    N['S6_MODELS'] = f"{sm['models_A']:,}"; N['S6_B_MODELS'] = sm['models_B']; N['S6_ALL'] = f"{sm['models']:,}"
    for k, p in [('EC', 'E|C'), ('LL', 'L|L2'), ('NN', 'N|N2'), ('EL', 'E|L'), ('LN', 'L|N'), ('EN', 'E|N')]:
        i, n = ok(p); N[f'S6_{k}'] = f'{i:,}'; N[f'S6_{k}_N'] = f'{n:,}'
    cl = sm['classes']['L|N']
    N['S6_LN_LAST'] = cl['last digits']; N['S6_LN_SMALL'] = cl['under 1 %']; N['S6_LN_MID'] = cl['1 to 10 %']; N['S6_LN_BIG'] = cl['over 10 %']
    N['S6_LN_OFF'] = cl['off-network']; N['S6_LN_MIDALL'] = _words(cl['last digits'] + cl['under 1 %'] + cl['1 to 10 %'])
    lg = _J(f'{U}/ln_groups.json'); gc = lg['counts']; pc1 = lg['pump1_control']
    N['S6_P1_DIFF_W'] = _words(gc['pump1_junction']); N['S6_CR_DIFF'] = _words(gc['code_review'], cap=True)
    fd_n = int(str(base_num['FULL_DIFF']).replace(',', ''))
    N['FULL_DIFF_TXT'] = 'none differ' if fd_n == 0 else f"{base_num['FULL_DIFF']} differ"
    N['S6_P1_STOR'] = f"{pc1.get('storage identical', 0) + pc1.get('storage differs', 0):,}"
    assert pc1.get('storage differs', 0) == 0, 'the pump control no longer holds; reword the essay'
    N['S6_OTHER_DIFF'] = _words(gc['other'])
    oth = lg['groups']['other']
    n193 = sum(1 for m in oth if '193_H&H_SI_Units' in m)
    N['S6_OTHER_TXT'] = (f"a training model and {_words(n193)} copies of one kinematic-wave network in SI units, not traced"
                         if n193 == len(oth) - 1 else ', '.join(oth))
    ce = sm['classes']['E|L']
    N['S6_EL_BIG'] = ce['over 10 %']; N['S6_EL_OFF'] = ce['off-network']
    N['S6_EXTRA'] = f"{sm['extra_period']:,}"
    sy = sm['system']
    N['S6_EN_VOL01'] = f"{sy['E|N']['volume_under_0.1']:,}"; N['S6_EN_VOLN'] = f"{sy['E|N']['n_volume']:,}"; N['S6_EN_VOL1'] = f"{sy['E|N']['volume_under_1']:,}"
    N['S6_EN_PK1'] = f"{sy['E|N']['peak_under_1']:,}"
    N['S6_LN_VOL01'] = f"{sy['L|N']['volume_under_0.1']:,}"; N['S6_LN_VOLN'] = f"{sy['L|N']['n_volume']:,}"
    # ---------------------------------------------------------------- the rerun: flag (A->B) and code (B->C)
    bu = _J(f'{U}/builds.json'); de = _J(f'{U}/deltas.json'); mv = _J(f'{U}/moves.json')
    N['UP_COMMITS'] = bu['engine_source_change']['commits']
    es = bu['engine_source_change']['src_engine_excluding_2d']
    N['UP_ENG_FILES'] = es['files']; N['UP_ENG_PLUS'] = f"{es['insertions']:,}"; N['UP_ENG_MINUS'] = f"{es['deletions']:,}"
    N['UP_FMA'] = f"{bu['A']['fma_new_engine']:,}"; N['UP_FMA_B'] = _words(bu['B']['fma_new_engine'])
    bn = de['build']['new']; blg = de['build']['legacy']
    assert set(blg) <= {'identical bytes', 'identical', 'missing', 'none'}, blg
    N['UP_NB_LAST'] = bn.get('last digits', 0); N['UP_NB_SMALL'] = bn.get('under 1 %', 0); N['UP_NB_MID'] = bn.get('1 to 10 %', 0)
    N['UP_NB_BIG'] = bn.get('over 10 %', 0); N['UP_NB_OFF'] = bn.get('off-network', 0)
    N['UP_NB_CHANGED'] = sum(bn.get(k, 0) for k in ('last digits', 'under 1 %', '1 to 10 %', 'over 10 %', 'off-network'))
    def ident(s, p): return s['status'][p].get('identical values', 0)
    def diff(s, p): return s['status'][p].get('differs', 0)
    N['UP_A_LN'] = f"{ident(sA, 'L|N'):,}"; N['UP_B_LN'] = f"{ident(sB, 'L|N'):,}"
    N['UP_A_EN'] = f"{ident(sA, 'E|N'):,}"; N['UP_B_EN'] = f"{ident(sB, 'E|N'):,}"
    N['UP_A_LN_DIFF'] = f"{diff(sA, 'L|N'):,}"; N['UP_B_LN_DIFF'] = f"{diff(sB, 'L|N'):,}"
    N['UP_FLAG_LN'] = f"{diff(sA, 'L|N') - diff(sB, 'L|N'):,}"
    N['UP_FLAG_BIG'] = f"{bn.get('over 10 %', 0):,}"
    back = sum(v for k, v in de['moves']['L|N']['A->B'].items() if k.startswith('identical ->') and not k.endswith('identical'))
    back += sum(v for k, v in de['moves']['E|N']['A->B'].items() if k.startswith('identical ->') and not k.endswith('identical'))
    assert back == 0, 'some model left identity when contraction was turned off; reword the essay'
    fp = _J(f'{U}/flag_features.json')
    N['UP_FL_PUMP'] = fp['moved_over10']['pumps']; N['UP_UN_PUMP'] = fp['unmoved']['pumps']
    N['UP_FL_CTL'] = fp['moved_over10']['controls']; N['UP_UN_CTL'] = fp['unmoved']['controls']
    eL, eN = mv['engines']['legacy'], mv['engines']['new']
    N['UP_CODE_L'] = eL['changed']; N['UP_CODE_N'] = eN['changed']
    assert mv['both']['only_legacy'] == 0 and mv['both']['same_verdict'] == mv['both']['n']
    N['UP_FURTHER'] = eN['counts'].get('further', 0); N['UP_CLOSER'] = eN['counts'].get('closer', 0)
    # the six models whose inputs other corpus models write
    fd = [l.strip() for l in open(f'{U}/file_dependent.txt') if l.strip()]
    N['UP_SKIP_TXT'] = (f"{_words(len(fd), cap=True)} models read files that other corpus models write, hot-start and "
                        "interface files, and EPA 5.2.4's own output for them changed between the October and November "
                        "runs; they are left out of every count of models that moved between the two dates.")
    lset = {x['inp'] for x in eL['models']}
    only = [x for x in eN['models'] if x['inp'] not in lset]
    oc = collections.Counter(x['verdict'] for x in only)
    own = sum(1 for x in only if x['verdict'] == 'no EPA' and x['inp'].startswith('OpenSWMM_Engine_Test_Decks/'))
    N['UP_CODE_ONLYN'] = (f"The other {_words(len(only))} changed in the new engine only. {_words(oc.get('no EPA', 0), cap=True)} "
                          f"have no EPA 5.2.4 result to measure against, {_words(own)} of them SWMM6's own test decks for "
                          f"features 5.2.4 lacks; {_words(oc.get('about same', 0) + oc.get('closer', 0) + oc.get('further', 0))} "
                          f"changed without moving their distance to EPA; and {_words(oc.get('now refused', 0))} are now refused. "
                          f"Runs that hit the five-minute limit in either build are not counted as changes.")
    assert oc.get('closer', 0) == 0 and oc.get('further', 0) == 0, oc
    bc = f'{U}/bisect.jsonl.changes.json'
    if os.path.exists(bc):
        from bisect_text import bisect_numbers
        n_listed = sum(1 for l in open(f'{U}/bisect_models_epa.txt') if l.strip())
        c_src = _J(bc)['commits'] - 1          # the commits after the old one that touch src, include or CMake
        nb, order, CC, by_model, verdict = bisect_numbers(_J(bc), mv, _J(f'{U}/losses_40f439a.json'), _words,
                                                          n_listed, c_src)
        N.update(nb)
        N.update(rest_text(order, CC, by_model, verdict, _words))
    else:
        for k in ('UP_COMMITS_TXT', 'UP_WHO_SHORT', 'UP_BISECT_N', 'UP_BISECT_LIMIT', 'UP_REST_TXT', 'UP_SRC_COMMITS',
                  'UP_BISECT_BUILT', 'UP_C1_N', 'UP_C1_FURTHER', 'UP_C1_CLOSER', 'UP_C1_SAME', 'UP_C1_FROM_EXACT',
                  'UP_C1_MAXF', 'UP_C1_NOLOSS', 'UP_C1_LOSSY', 'UP_C1_PUMPS', 'UP_C1_CTL', 'UP_C1_CONT_SAME',
                  'UP_C1_WORST_MODEL', 'UP_C1_WORST_B', 'UP_C1_WORST_C', 'UP_C1_WORST_D0', 'UP_C1_WORST_D1'):
            N[k] = '[bisection pending]'
    # trace
    N['S6_TR_MODEL'] = '<code>Exercise0_5_H&amp;H_Elements.inp</code>'; N['S6_TR_MODEL_TXT'] = 'Exercise0_5_H&H_Elements.inp'
    tm = [l.strip() for l in open(os.path.join(os.path.dirname(os.path.dirname(D)), 'tools/perf/models.txt')) if l.strip()] if os.path.exists(os.path.join(os.path.dirname(os.path.dirname(D)), 'tools/perf/models.txt')) else []
    N['S6_MAX_LINKS'] = f"{max((links or {}).get(m, 0) for m in tm):,}" if tm and links else '333'
    N['S6_TR_STEP'] = tr['first_step']; N['S6_TR_STEPS'] = f"{round(tr['steps_legacy'], -2):,}"; N['S6_TR_T'] = f"{tr['first_step_time_s']:.0f}"
    N['S6_TR_LINK'] = tr['link_id']; N['S6_TR_FIRST'] = f"{tr['first_report_period_differing_min']:.0f}"
    N['S6_TR_DQ'] = f"{tr['max_dflow_cfs']:.1f}"; N['S6_TR_Q'] = f"{tr['peak_flow_cfs']:,.0f}"; N['S6_TR_DY'] = f"{tr['max_ddepth_ft']:.2f}"
    N['S6_TR_SAME'] = tr['periods_identical']; N['S6_TR_PER'] = tr['periods']
    # refusals
    pat, cases = sm['outputs']['pattern'], sm['outputs']['cases']
    refused = cases.get('EL-', [])
    import re as _re
    kinds = collections.OrderedDict()
    for m, e, rcode in refused:
        if e:
            code = _re.match(r'(ERROR \d+)', e).group(1); msg = e.split(':', 1)[1].strip().rstrip('.')
            msg = _re.sub(r' (Outfall|object) \S+', r' \1 X', msg); msg = _re.sub(r' file .*$', ' file', msg)
            kinds.setdefault(code, [0, msg])[0] += 1
        else:
            kinds.setdefault('crash' if rcode != 'timeout' else 'timeout', [0, ''])[0] += 1
    top = sorted(((v[0], k, v[1]) for k, v in kinds.items() if k.startswith('ERROR')), reverse=True)
    t3 = top[:3]; rest = sum(n for n, _, _ in top[3:])
    tos = sm.get('timeouts', {}).get('N', [])
    crash = kinds.get('crash', [0])[0]
    N['S6_N_REFUSE'] = len(refused)
    N['S6_N_REFUSE_TXT'] = (f"SWMM6's new engine wrote no results for {len(refused)} models that EPA 5.2.4 and the legacy engine both run. "
        + 'Most stop at input checks the older engines do not make or let pass: ' + '; '.join(f'{n} with {k}, &ldquo;{msg.lower()}&rdquo;' for n, k, msg in t3)
        + (f'; and {rest} with other input errors' if rest else '') + '. ' 
        + _timeouts_text(refused, sm))
    both = cases.get('-LN', []); only = cases.get('--N', [])
    own = sum(1 for m, e, rc in both if m.startswith('OpenSWMM_Engine_Test_Decks/'))
    N['S6_SIX_TXT'] = (f"Going the other way, {len(both)} models run in both SWMM6 engines and stop EPA 5.2.4, {own} of them SWMM6's own "
                       f"test decks written with keywords 5.2.4 does not know, and {len(only)} more run only in the new engine.")
    N['S6_TIMEOUTS'] = sm.get('timeouts', {})
    # attribution
    if att:
        rk = att.get('rest_kind', {})
        rest = att['n'] - att['outfall'] - att['more']
        N['S6_OF_TXT'] = (f"How much does that one rule matter? Change only those lines in EPA 5.2.4 and, of the {att['n']:,} corpus models "
                          f"where the legacy engine at c40580c disagreed with 5.2.4, {att['outfall']:,} come out identical to it. Add the storage and "
                          f"conveyance changes and {att['outfall'] + att['more']:,} do. Of the other {rest:,}, "
                          f"{rk.get('runoff, inflow or quality values differ', 0):,} show differences in runoff, inflow or water-quality "
                          f"values and {rk.get('hydraulic values only', 0)} in hydraulics alone. Those come from the legacy engine's other "
                          f"changes: {N['S6_LEG_FILES']} of its {N['S6_LEG_FILES_N']} source files differ from 5.2.4 in some way, renamed "
                          f"variables included, and none of those changes were traced further.")
    else:
        N['S6_OF_TXT'] = ''
    pr = _J(f'{D}/profile_10subs.json') if os.path.exists(f'{D}/profile_10subs.json') else None
    if pr:
        g = lambda f, *ks: sum(pr[f]['by'].get(k, 0) for k in ks) / 1e9
        N['S6_PR_MODEL'] = '<code>10_Subs_Continuous.inp</code>'
        N['S6_PR_EPA'] = f"{pr['epa']['total'] / 1e9:.1f}"; N['S6_PR_NEW'] = f"{pr['new']['total'] / 1e9:.1f}"
        N['S6_PR_HYD_EPA'] = f"{g('epa', 'routing', 'geometry'):.1f}"; N['S6_PR_HYD_NEW'] = f"{g('new', 'routing', 'geometry'):.1f}"
        N['S6_PR_OUT_EPA'] = f"{g('epa', 'output'):.2f}"; N['S6_PR_OUT_NEW'] = f"{g('new', 'output'):.1f}"
        N['S6_PR_MEM_EPA'] = f"{g('epa', 'memory'):.2f}"; N['S6_PR_MEM_NEW'] = f"{g('new', 'memory'):.1f}"
    bb = _J(f'{D}/bench_buffer.json') if os.path.exists(f'{D}/bench_buffer.json') else None
    bi = _J(f'{D}/buffer_identity.json') if os.path.exists(f'{D}/buffer_identity.json') else []
    if bb:
        gb, perb = ratios(bb, 'swmm6_new_buffered'); g0, per0 = ratios(bb, 'swmm6_new')
        k = bb['models'].index('Hydrology/10_Subs_Continuous.inp'); w = bb['wall_median']['Hydrology/10_Subs_Continuous.inp']
        n_id = sum(1 for r in bi if r['same_out'] and r['same_rpt']) + sum(1 for v in bb['identical_new_vs_buffered'].values() if v)
        n_all = len(bi) + len(bb['identical_new_vs_buffered'])
        N['S6_BUF_TXT'] = (f"That is cheap to test, so I tested it, on the first build. A copy of SWMM6 patched to gather each reporting period into one "
                           f"buffer and write it with one call, flush kept, gave byte-identical output on {sum(1 for r in bi if r['same_out'] and r['same_rpt'])} of {len(bi)} models checked. "
                           f"Timed again alongside the unpatched build, the continuous model's CPU time fell from {per0[k]:.2f} to {perb[k]:.2f} times EPA's. The time on the "
                           f"clock hardly moved, {w['swmm6_new']:.2f} seconds against {w['swmm6_new_buffered']:.2f}, because the writing "
                           f"happens on the background thread while the solver runs on. On the other seven models the change was inside "
                           f"the noise. So the fix is real and cheap, and it is not where the wait is. The wait is in the rest of the "
                           f"bookkeeping around the solver, the allocating, copying and statistics, which no one-line patch removes.")
    else:
        N['S6_BUF_TXT'] = ''
    bl = (_alias(_J(f'{U}/bench6u_large.json')) if os.path.exists(f'{U}/bench6u_large.json')
          else _J(f'{D}/bench_large.json'))
    if bl:
        ms = list(bl['results']); lk = bl['links']
        gN, perN = ratios(bl, 'swmm6_new'); gL, perL = ratios(bl, 'swmm6_legacy'); gC, perC = ratios(bl, 'cplus_lto'); _, nz = ratios(bl, 'ref2')
        big = max(ms, key=lambda m: lk[m]); ib = ms.index(big)
        mid = [i for i, m in enumerate(ms) if m != big]
        N['S6_LG_N'] = {3: 'three', 4: 'four', 5: 'five', 6: 'six', 7: 'seven', 8: 'eight'}.get(len(ms), len(ms)); N['S6_LG_MIN'] = f"{min(lk[m] for m in ms):,}"; N['S6_LG_MAX'] = f"{lk[big]:,}"
        N['S6_LG_NEW'] = f"{gN:.2f}"; N['S6_LG_BIG'] = f"{perN[ib]:.2f}"
        N['S6_LG_MID'] = f"{min(perN[i] for i in mid):.2f} to {max(perN[i] for i in mid):.2f}"
        N['S6_LG_MIDLINKS'] = f"{min(lk[ms[i]] for i in mid):,} to {max(lk[ms[i]] for i in mid):,}"
        N['S6_LG_LEG'] = f"{gL:.2f}"; N['S6_LG_CP'] = f"{gC:.2f}"; N['S6_LG_NOISE'] = f"{min(nz):.2f} to {max(nz):.2f}"
        # the average routing step on the biggest network, from the gate's reports
        import gzip as _gz
        st_big = None
        for l in _gz.open(f'{U}/gate6u.jsonl.gz', 'rt'):
            r = json.loads(l)
            if r['inp'] == big: st_big = {k: (v or {}).get('avgStep') for k, v in r['rpt'].items()}; break
        faster = perN[ib] < min(nz)
        txt = 'faster than EPA, by more than the noise' if faster else ('within the noise of EPA' if perN[ib] <= max(nz) else 'slower than EPA')
        if st_big and st_big.get('NC') == st_big.get('E'):
            txt += f", with the same average routing step as EPA, {st_big['E']:.2f} seconds."
            if st_big.get('NA') and st_big['NA'] != st_big['E']:
                txt += (f" The first build, with fused instructions, had averaged {st_big['NA']:.2f} seconds there, "
                        "so the extra steps the first version of this essay reported on that network came from the flag.")
        else:
            txt += '.'
        N['S6_LG_BIG_TXT'] = txt
    # speed
    figs = dict(s6_layout=F6.fig_s6_layout(), s6_parity=F6.fig_s6_parity(st, sm),
                s6_classes=F6.fig_s6_classes(sm), s6_growth=F6.fig_s6_growth(growth, 'Exercise0_5'))
    ec = sC['status']['E|C']
    figs['flags'] = F6.fig_s6_flags(sA, sB, sC, ec.get('identical values', 0), ec.get('identical values', 0) + ec.get('differs', 0))
    if bench:
        rows = []
        noise = ratios(bench, 'ref2')[1]
        for lab, b, col in [('swmm5-cplus R4, -O2 -flto', 'cplus_lto', 'var(--f-c)'), ('swmm5-cplus R4, -O3 LTO + PGO', 'cplus_pgo', 'var(--f-c)'),
                            ('SWMM6 legacy engine, db4634a', 'swmm6_legacy', 'var(--f-b)'),
                            ('SWMM6 new, c40580c, contraction on', 'swmm6_new_A', 'var(--f-e)'),
                            ('SWMM6 new, c40580c, contraction off', 'swmm6_new_B', 'var(--f-e)'),
                            ('SWMM6 new engine, db4634a', 'swmm6_new', 'var(--f-a)')]:
            if b not in next(iter(bench['results'].values())): continue
            g, per = ratios(bench, b); rows.append((lab, g, per, col)); N['S6_SP_' + b.upper()] = f'{g:.2f}'
            N['S6_SP_' + b.upper() + '_RANGE'] = f'{min(per):.2f} to {max(per):.2f}'
        N['S6_SP_NOISE'] = f'{min(noise):.2f} to {max(noise):.2f}'
        r0 = next(iter(bench['results'].values()))
        if 'swmm6_new_A' in r0 and 'swmm6_new_B' in r0:
            gA, pA = ratios(bench, 'swmm6_new_A'); gB, pB = ratios(bench, 'swmm6_new_B')
            ab = [b / a for a, b in zip(pA, pB)]
            straddle = min(ab) < 1 < max(ab)
            N['S6_SP_AB_TXT'] = (f"The first build, with fused instructions, took {gA:.2f}; the same commit without them took "
                                 f"{gB:.2f}. Model by model the second took {min(ab):.2f} to {max(ab):.2f} times the first's time"
                                 + (", on both sides of 1, so the fused instructions bought no speed this method can see."
                                    if straddle else '.'))
        else:
            N['S6_SP_AB_TXT'] = '[retiming pending]'
        N['S6_SP_WALL'] = f"{ratios(bench, 'swmm6_new', 'wall')[0]:.2f}"
        figs['s6_speed'] = F6.fig_s6_speed(rows, (min(noise), max(noise)))
        gN, perN = ratios(bench, 'swmm6_new'); gL, perL = ratios(bench, 'swmm6_legacy'); gP, perP = ratios(bench, 'cplus_pgo')
        models = list(bench['results'])
        worst = max(zip(perN, models)); best = min(zip(perN, models))
        N['S6_SPEED_TXT'] = bench.get('text', '')
        N['S6_SP_WORST'] = f'{worst[0]:.2f}'; N['S6_SP_WORST_M'] = F6.SHORT6.get(worst[1], worst[1])
        N['S6_SP_BEST'] = f'{best[0]:.2f}'; N['S6_SP_BEST_M'] = F6.SHORT6.get(best[1], best[1])
    return N, figs
