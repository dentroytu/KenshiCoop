// XferPair.h - pair settled inventory diffs into cross-container transfers
// (protocol 37 drag detector). Pure C++03 so prototest can lock it without the
// game; the plugin feeds it per-container, per-item diffs against its baseline.
//
// Why a separate pass: the detector used to fire ONE move per container per
// scan, then re-capture both containers whole - so every other item moved in
// the same ~1 s (a player dragging three stacks into a chest) was folded into
// the baseline unannounced. On the owner's next snapshot those items were
// re-added (duplicated) or wiped (lost). Every settled loss is now paired, a
// loss may split across several gains, and each unit is paired at most once.

#ifndef KENSHICOOP_XFERPAIR_H
#define KENSHICOOP_XFERPAIR_H

#include <map>
#include <utility>
#include <vector>

namespace coop {

template <class K, class X>
struct XferFire {
    K   src;
    K   dst;
    X   key;
    int qty;
};

// `settled` holds, per container, the SETTLED diff of each item key against the
// detector's baseline: < 0 is a loss, > 0 a gain (unsettled diffs are left out
// by the caller). Losses are walked in map order and matched against gains of
// the same key in OTHER containers, also in map order, so the result is
// deterministic. A gain that no loss claims stays unpaired (loot, crafting).
template <class K, class X>
void pairXferDiffs(const std::map<K, std::map<X, int> >& settled,
                   std::vector<XferFire<K, X> >& out) {
    typedef typename std::map<K, std::map<X, int> >::const_iterator CIt;
    typedef typename std::map<X, int>::const_iterator EIt;
    out.clear();
    std::map<std::pair<K, X>, int> usedGain;
    for (CIt s = settled.begin(); s != settled.end(); ++s) {
        for (EIt e = s->second.begin(); e != s->second.end(); ++e) {
            if (e->second >= 0) continue;
            int need = -e->second;
            for (CIt d = settled.begin(); d != settled.end() && need > 0; ++d) {
                if (d == s) continue;
                EIt g = d->second.find(e->first);
                if (g == d->second.end() || g->second <= 0) continue;
                int& used = usedGain[std::make_pair(d->first, e->first)];
                const int avail = g->second - used;
                if (avail <= 0) continue;
                const int q = (need < avail) ? need : avail;
                XferFire<K, X> f;
                f.src = s->first; f.dst = d->first; f.key = e->first; f.qty = q;
                out.push_back(f);
                used += q;
                need -= q;
            }
        }
    }
}

} // namespace coop

#endif // KENSHICOOP_XFERPAIR_H
