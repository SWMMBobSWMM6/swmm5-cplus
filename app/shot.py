import asyncio, sys, json
from playwright.async_api import async_playwright
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch()
        pg = await b.new_page(viewport={'width': 1280, 'height': 900})
        logs = []
        pg.on('console', lambda m: logs.append(f'{m.type}: {m.text}'))
        pg.on('pageerror', lambda e: logs.append(f'PAGEERROR: {e}'))
        await pg.goto('file:///home/claude/appbuild/swmm5-cplus.html')
        await pg.wait_for_timeout(1500)
        await pg.click('#welcomeClose')
        await pg.wait_for_function("document.querySelector('#verdictBig').textContent !== 'Waiting for the first run'", timeout=60000)
        print('verdict:', await pg.text_content('#verdictBig'), '|', await pg.text_content('#verdictLine'))
        print('diff:', await pg.text_content('#diffNote'))
        await pg.screenshot(path='shots/run_dark.png', full_page=True)
        # many
        await pg.click('[data-view="many"]'); await pg.click('#manyBtn')
        await pg.wait_for_function("document.querySelector('#manyStatus').textContent === 'Done.' || document.querySelector('#manyStatus').textContent.startsWith('Failed')", timeout=180000)
        print('many:', await pg.text_content('#manyFoot'), await pg.text_content('#manyStatus'))
        await pg.click('#twoBtn'); await pg.wait_for_timeout(4000)
        print('two:', (await pg.text_content('#twoOut'))[:300])
        await pg.screenshot(path='shots/many_dark.png', full_page=True)
        await pg.click('[data-view="coupled"]'); await pg.click('#coupleBtn')
        await pg.wait_for_function("document.querySelector('#coupleStatus').textContent.startsWith('Done') || document.querySelector('#coupleStatus').textContent.startsWith('Failed')", timeout=120000)
        print('coupled:', await pg.text_content('#coupleStatus'), await pg.text_content('#coupleTiles'))
        await pg.screenshot(path='shots/coupled_dark.png', full_page=True)
        for v in ['inside', 'verify', 'about', 'doc']:
            await pg.click(f'[data-view="{v}"]'); await pg.wait_for_timeout(400)
            await pg.screenshot(path=f'shots/{v}_dark.png', full_page=(v != 'doc'))
        await pg.click('#theme-toggle [data-t="light"]'); await pg.wait_for_timeout(300)
        for v in ['run', 'verify', 'doc']:
            await pg.click(f'[data-view="{v}"]'); await pg.wait_for_timeout(300)
            await pg.screenshot(path=f'shots/{v}_light.png', full_page=False)
        m = await b.new_page(viewport={'width': 390, 'height': 844})
        await m.goto('file:///home/claude/appbuild/swmm5-cplus.html'); await m.wait_for_timeout(1500)
        await m.click('#welcomeClose'); await m.wait_for_timeout(2500)
        sw = await m.evaluate('document.documentElement.scrollWidth')
        print('mobile scrollWidth', sw)
        await m.screenshot(path='shots/run_mobile.png', full_page=False)
        print('\n'.join(logs[:30]))
        await b.close()
asyncio.run(main())
