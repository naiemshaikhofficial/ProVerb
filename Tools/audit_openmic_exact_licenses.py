import csv
from collections import Counter, defaultdict

CSV_PATH = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/openmic-2018-metadata.csv'

def main():
    pairs = Counter()
    urls = Counter()
    titles = Counter()
    url_to_titles = defaultdict(set)
    title_to_urls = defaultdict(set)

    with open(CSV_PATH, mode='r', encoding='utf-8', errors='ignore') as f:
        reader = csv.DictReader(f)
        for row in reader:
            lt = row.get('license_title', '').strip()
            lu = row.get('license_url', '').strip()
            pairs[(lt, lu)] += 1
            urls[lu] += 1
            titles[lt] += 1
            url_to_titles[lu].add(lt)
            title_to_urls[lt].add(lu)

    print("=== ALL LICENSES IN OPENMIC-2018 ===")
    print(f"Total rows: {sum(pairs.values())}")
    print(f"Total distinct titles: {len(titles)}")
    print(f"Total distinct URLs: {len(urls)}")
    print(f"Total distinct (title, url) pairs: {len(pairs)}\n")

    # Inspect licenses matching England or UK
    print("=== LICENSES WITH 'UK' OR 'ENGLAND' ===")
    for (lt, lu), count in pairs.items():
        if 'uk' in lt.lower() or 'england' in lt.lower() or 'uk' in lu.lower() or 'england' in lu.lower():
            print(f"  Count: {count} | Title: '{lt}' | URL: '{lu}'")

    # Let's inspect all non-commercial / commercial classifications
    # Disqualifiers: nc, noncommercial, non-commercial, nd, noderivs, no-derivatives, no derivative, sa, sharealike, share-alike, share alike, sampling
    def is_disqualified(title, url):
        t_low = title.lower()
        u_low = url.lower()
        disqualifiers = ['-nc', 'noncommercial', 'non-commercial', '-nd', 'noderiv', 'no derivative', '-sa', 'sharealike', 'share-alike', 'share alike', 'sampling']
        for d in disqualifiers:
            if d in t_low or d in u_low:
                return True, d
        return False, None

    commercial_pairs = {}
    quarantined_pairs = {}

    for (lt, lu), count in pairs.items():
        disq, reason = is_disqualified(lt, lu)
        if disq:
            quarantined_pairs[(lt, lu)] = (count, reason)
        else:
            commercial_pairs[(lt, lu)] = count

    print("\n=== COMMERCIAL CANDIDATES (EXACT TITLE + URL) ===")
    tot_comm = 0
    for (lt, lu), count in sorted(commercial_pairs.items(), key=lambda x: -x[1]):
        tot_comm += count
        print(f"  {count:4d} tracks | Title: '{lt}' | URL: '{lu}'")
    print(f"Total Commercial Candidates: {tot_comm}")
    return

    print("\n=== QUARANTINED (EXACT TITLE + URL + REASON) ===")
    tot_quar = 0
    for (lt, lu), (count, reason) in sorted(quarantined_pairs.items(), key=lambda x: -x[1][0]):
        tot_quar += count
        print(f"  {count:5d} tracks | Reason: '{reason}' | Title: '{lt}' | URL: '{lu}'")
    print(f"Total Quarantined: {tot_quar}")

if __name__ == '__main__':
    main()
