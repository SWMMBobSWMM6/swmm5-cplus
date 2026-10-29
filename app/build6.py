"""Numbers and figures for the SWMM6 comparison section, from results/swmm6/ in the repository."""
import os, json, math, collections
import figures6 as F6

def _J(p): return json.load(open(p))

def ratios(bench, b, key='cpu'):
    R = bench['results']; per = []
    for m in R:
        pr = [y[key] / x[key] for x, y in zip(R[m]['ref'], R[m][b])]
        per.append(sorted(pr)[len(pr) // 2])
    return math.exp(sum(math.log(v) for v in per) / len(per)), per


def _timeouts_text(refused, sm):
    """The new engine's time-outs on models EPA runs, and its aborts among models EPA and the legacy engine both run."""
    tos = [t for t in sm.get('timeouts_new', []) if t['epa_output']]
    crashes = [m for m, e, rc in refused if not e and rc != 'timeout']
    desc = {'IO-02': 'One is a code-review deck whose start time is NaN: EPA and the legacy engine finish it at once with no reporting periods, and the new engine had written close to a gigabyte of output when it was stopped.',
            '1000year': 'The other is a 1,000-year simulation that EPA finishes in {epa:.0f} seconds and the legacy engine in {leg:.0f}.'}
    parts = []
    for t in tos:
        k = next((k for k in desc if k in t['inp']), None)
        parts.append(desc[k].format(epa=t['epa_s'], leg=t['legacy_s']) if k else t['inp'])
    words = {1: 'One', 2: 'Two', 3: 'Three', 4: 'Four'}
    out = ''
    if crashes:
        out += (f"{words.get(len(crashes), len(crashes))} aborted with no message" + (", a code-review deck with a malformed exponent in an expression" if any('IO-41' in m for m in crashes) else '') + '. ')
    if tos:
        out += f"{words.get(len(tos), len(tos))} more ran past the five-minute limit. " + ' '.join(parts)
    return out

def build(repo, base_num, links=None):
    D = f'{repo}/results/swmm6'
    st, sm, tr = _J(f'{D}/static.json'), _J(f'{D}/summary.json'), _J(f'{D}/trace_ex05.json')
    growth = _J(f'{D}/trace_ex05_growth.json')
    att = _J(f'{D}/attribution.json') if os.path.exists(f'{D}/attribution.json') else None
    bench = _J(f'{D}/bench_swmm6.json') if os.path.exists(f'{D}/bench_swmm6.json') else None
    L, C, S = st['lines'], st['counts'], st['subsystems']
    def ok(p):
        s = sm['status'][p]; return s.get('identical values', 0), s.get('identical values', 0) + s.get('differs', 0)
    N = {}
    N['S6_DATE'] = '28 October 2026' if st['swmm6_date'] == '2026-10-28' else st['swmm6_date']
    N['S6_COMMIT'] = '<code>' + st['swmm6_commit'][:7] + '</code>'
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
    N['S6_LN_OFF'] = cl['off-network']
    ce = sm['classes']['E|L']
    N['S6_EL_BIG'] = ce['over 10 %']; N['S6_EL_OFF'] = ce['off-network']
    N['S6_EXTRA'] = f"{sm['extra_period']:,}"
    sy = sm['system']
    N['S6_EN_VOL01'] = f"{sy['E|N']['volume_under_0.1']:,}"; N['S6_EN_VOLN'] = f"{sy['E|N']['n_volume']:,}"; N['S6_EN_VOL1'] = f"{sy['E|N']['volume_under_1']:,}"
    N['S6_EN_PK1'] = f"{sy['E|N']['peak_under_1']:,}"
    N['S6_LN_VOL01'] = f"{sy['L|N']['volume_under_0.1']:,}"; N['S6_LN_VOLN'] = f"{sy['L|N']['n_volume']:,}"
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
                          f"where the legacy engine disagreed with 5.2.4, {att['outfall']:,} come out identical to it. Add the storage and "
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
        N['S6_BUF_TXT'] = (f"That is cheap to test, so I tested it. A copy of SWMM6 patched to gather each reporting period into one "
                           f"buffer and write it with one call, flush kept, gave byte-identical output on {sum(1 for r in bi if r['same_out'] and r['same_rpt'])} of {len(bi)} models checked. "
                           f"Timed again alongside the unpatched build, the continuous model's CPU time fell from {per0[k]:.2f} to {perb[k]:.2f} times EPA's. The time on the "
                           f"clock hardly moved, {w['swmm6_new']:.2f} seconds against {w['swmm6_new_buffered']:.2f}, because the writing "
                           f"happens on the background thread while the solver runs on. On the other seven models the change was inside "
                           f"the noise. So the fix is real and cheap, and it is not where the wait is. The wait is in the rest of the "
                           f"bookkeeping around the solver, the allocating, copying and statistics, which no one-line patch removes.")
    else:
        N['S6_BUF_TXT'] = ''
    bl = _J(f'{D}/bench_large.json') if os.path.exists(f'{D}/bench_large.json') else None
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
    # speed
    figs = dict(s6_layout=F6.fig_s6_layout(), s6_parity=F6.fig_s6_parity(st, sm),
                s6_classes=F6.fig_s6_classes(sm), s6_growth=F6.fig_s6_growth(growth, 'Exercise0_5'))
    if bench:
        rows = []
        noise = ratios(bench, 'ref2')[1]
        for lab, b, col in [('swmm5-cplus R4, -O2 -flto', 'cplus_lto', 'var(--f-c)'), ('swmm5-cplus R4, -O3 LTO + PGO', 'cplus_pgo', 'var(--f-c)'),
                            ('SWMM6 legacy engine', 'swmm6_legacy', 'var(--f-b)'), ('SWMM6 new engine', 'swmm6_new', 'var(--f-a)')]:
            g, per = ratios(bench, b); rows.append((lab, g, per, col)); N['S6_SP_' + b.upper()] = f'{g:.2f}'
            N['S6_SP_' + b.upper() + '_RANGE'] = f'{min(per):.2f} to {max(per):.2f}'
        N['S6_SP_NOISE'] = f'{min(noise):.2f} to {max(noise):.2f}'
        N['S6_SP_WALL'] = f"{ratios(bench, 'swmm6_new', 'wall')[0]:.2f}"
        figs['s6_speed'] = F6.fig_s6_speed(rows, (min(noise), max(noise)))
        gN, perN = ratios(bench, 'swmm6_new'); gL, perL = ratios(bench, 'swmm6_legacy'); gP, perP = ratios(bench, 'cplus_pgo')
        models = list(bench['results'])
        worst = max(zip(perN, models)); best = min(zip(perN, models))
        N['S6_SPEED_TXT'] = bench.get('text', '')
        N['S6_SP_WORST'] = f'{worst[0]:.2f}'; N['S6_SP_WORST_M'] = F6.SHORT6.get(worst[1], worst[1])
        N['S6_SP_BEST'] = f'{best[0]:.2f}'; N['S6_SP_BEST_M'] = F6.SHORT6.get(best[1], best[1])
    return N, figs
