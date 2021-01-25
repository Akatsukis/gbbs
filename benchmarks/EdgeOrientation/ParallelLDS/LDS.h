// This code is part of the project "Theoretically Efficient Parallel Graph
// Algorithms Can Be Fast and Scalable", presented at Symposium on Parallelism
// in Algorithms and Architectures, 2018.
// Copyright (c) 2018 Laxman Dhulipala, Guy Blelloch, and Julian Shun
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all  copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <unordered_set>
#include <stack>

#include "gbbs/gbbs.h"

#include "sparse_set.h"

namespace gbbs {

struct LDS {
  size_t n;  // number of vertices
  static constexpr double delta = 9.0;
  static constexpr double upper_constant = (2 + static_cast<double>(3) / delta);
  static constexpr double epsilon = 3.0;
  static constexpr double one_plus_eps = 1 + epsilon;

  static constexpr uintE kUpLevel = UINT_E_MAX;

  using levelset = gbbs::sparse_set<uintE>;
  using down_neighbors = parlay::sequence<levelset>;
  using up_neighbors = levelset;
  using edge_type = std::pair<uintE, uintE>;

  struct LDSVertex {
    uintE level;  // level of this vertex
    down_neighbors
        down;         // neighbors in levels < level, bucketed by their level.
    up_neighbors up;  // neighbors

    LDSVertex() : level(0) {}

//    void insert_neighbor(uintE v, uintE l_v) {
//      if (l_v < level) {
//        assert(down.size() > l_v);
//        down[l_v].insert(v);
//      } else {
//        up.insert(v);
//      }
//    }
//
//    void remove_neighbor(uintE v, uintE l_v) {
//      if (l_v < level) {
//        down[l_v].erase(down[l_v].find(v));
//      } else {
//        up.erase(up.find(v));
//      }
//    }

    inline double group_degree(size_t group) const {
      return pow(one_plus_eps, group);
    }

    inline bool upper_invariant(const size_t levels_per_group) const {
      uintE group = level / levels_per_group;
      return up.size() <= static_cast<size_t>(upper_constant * group_degree(group));
    }

    inline bool lower_invariant(const size_t levels_per_group) const {
      if (level == 0) return true;
      uintE lower_group = (level - 1) / levels_per_group;
      auto up_size = up.size();
      auto prev_level_size = down[level - 1].size();
      return (up_size + prev_level_size) >= static_cast<size_t>(group_degree(lower_group));  // needs a floor or ceil?
    }

  };

  // number of inner-levels per group,  O(\log n) many.
  size_t levels_per_group;
  parlay::sequence<LDSVertex> L;

  LDS(size_t n) : n(n) {
    levels_per_group = ceil(log(n) / log(one_plus_eps));
    L = parlay::sequence<LDSVertex>(n);
  }

  uintE get_level(uintE ngh) {
    return L[ngh].level;
  }

//  // Moving u from level to level - 1.
//  template <class Levels>
//  void level_decrease(uintE u, Levels& L) {
//    uintE level = L[u].level;
//    auto& up = L[u].up;
//    assert(level > 0);
//    auto& prev_level = L[u].down[level - 1];
//
//    for (const auto& ngh : prev_level) {
//      up.insert(ngh);
//    }
//    L[u].down.pop_back();  // delete the last level in u's structure.
//
//    for (const auto& ngh : up) {
//      if (get_level(ngh) == level) {
//        L[ngh].up.erase(L[ngh].up.find(u));
//        L[ngh].down[level-1].insert(u);
//
//      } else if (get_level(ngh) > level) {
//        L[ngh].down[level].erase(L[ngh].down[level].find(u));
//        L[ngh].down[level - 1].insert(u);
//
//        if (get_level(ngh) == level + 1) {
//          Dirty.push(ngh);
//        }
//      } else {
//        // u is still "up" for this ngh, no need to update.
//        assert(get_level(ngh) == (level - 1));
//      }
//    }
//    L[u].level--;  // decrease level
//  }
//
//  // Moving u from level to level + 1.
//  template <class Levels>
//  void level_increase(uintE u, Levels& L) {
//    uintE level = L[u].level;
//    std::vector<uintE> same_level;
//    auto& up = L[u].up;
//
//    for (auto it = up.begin(); it != up.end();) {
//      uintE ngh = *it;
//      if (L[ngh].level == level) {
//        same_level.emplace_back(ngh);
//        it = up.erase(it);
//        // u is still "up" for this ngh, no need to update.
//      } else {
//        it++;
//        // Must update ngh's accounting of u.
//        if (L[ngh].level > level + 1) {
//          L[ngh].down[level].erase(L[ngh].down[level].find(u));
//          L[ngh].down[level + 1].insert(u);
//        } else {
//          assert(L[ngh].level == level + 1);
//          L[ngh].down[level].erase(L[ngh].down[level].find(u));
//          L[ngh].up.insert(u);
//
//          Dirty.push(ngh);
//        }
//      }
//    }
//    // We've now split L[u].up into stuff in the same level (before the
//    // update) and stuff in levels >= level + 1. Insert same_level elms
//    // into down.
//    auto& down = L[u].down;
//    down.emplace_back(std::unordered_set<uintE>());
//    assert(down.size() == level + 1);  // [0, level)
//    for (const auto& ngh : same_level) {
//      down[level].insert(ngh);
//    }
//    L[u].level++;  // Increase level.
//  }
//
//  void fixup() {
//    while (!Dirty.empty()) {
//      uintE u = Dirty.top();
//      Dirty.pop();
//      if (!L[u].upper_invariant(levels_per_group)) {
//        // Move u to level i+1.
//        level_increase(u, L);
//        Dirty.push(u);  // u might need to move up more levels.
//        // std::cout << "(move up) pushing u = " << u << std::endl;
//      } else if (!L[u].lower_invariant(levels_per_group)) {
//        level_decrease(u, L);
//        Dirty.push(u);  // u might need to move down more levels.
//        // std::cout << "(move down) pushing u = " << u << std::endl;
//      }
//    }
//  }

  bool edge_exists(edge_type e) {
    auto[u, v] = e;
    auto l_u = L[u].level;
    auto l_v = L[v].level;
    if (l_u < l_v) {  // look in up(u)
      return L[u].up.contains(v);
    } else {  // look in up(v)
      return L[v].up.contains(u);
    }
  }

//  bool insert_edge(edge_type e) {
//    if (edge_exists(e)) return false;
//    auto[u, v] = e;
//    auto l_u = L[u].level;
//    auto l_v = L[v].level;
//    L[u].insert_neighbor(v, l_v);
//    L[v].insert_neighbor(u, l_u);
//
//    Dirty.push(u); Dirty.push(v);
//    fixup();
//    return true;
//  }


  template <class Seq>
  void settle(Seq&& affected) {
    // use bucketing structure?

  }

  template <class Seq>
  void batch_insertion(const Seq& insertions_unfiltered) {
    // Remove edges that already exist from the input.
    auto insertions_filtered = parlay::filter(parlay::make_slice(insertions_unfiltered),
        [&] (const edge_type& e) { return !edge_exists(e); });

    // Duplicate the edges in both directions and sort.
    auto insertions_dup = parlay::sequence<edge_type>::uninitialized(2*insertions_filtered.size());
    parallel_for(0, insertions_filtered.size(), [&] (size_t i) {
      auto [u, v] = insertions_filtered[i];
      insertions_dup[2*i] = {u, v};
      insertions_dup[2*i + 1] = {v, u};
    });
    auto compare_tup = [&] (const edge_type& l, const edge_type& r) { return l < r; };
    parlay::sort_inplace(parlay::make_slice(insertions_dup), compare_tup);

    // Remove duplicate edges to get the insertions.
    auto not_dup_seq = parlay::delayed_seq<bool>(insertions_dup.size(), [&] (size_t i) {
      auto [u, v] = insertions_dup[i];
      bool not_self_loop = u != v;
      return not_self_loop && ((i == 0) || (insertions_dup[i] != insertions_dup[i-1]));
    });
    auto insertions = parlay::pack(parlay::make_slice(insertions_dup), not_dup_seq);


    // Compute the starts of each (modified) vertex's new edges.
    auto bool_seq = parlay::delayed_seq<bool>(insertions.size() + 1, [&] (size_t i) {
      return (i == 0) || (i == insertions.size()) || (std::get<0>(insertions[i-1]) != std::get<0>(insertions[i]));
    });
    auto starts = parlay::pack_index(bool_seq);

    // Save the vertex ids and starts (hypersparse CSR format). The next step
    // will overwrite the edge pairs to store (neighbor, current_level).
    // (saving is not necessary if we modify + sort in a single parallel loop)
    auto affected = parlay::sequence<uintE>::from_function(starts.size(), [&] (size_t i) {
      size_t idx = starts[i];
      uintE vtx_id = (i < insertions.size()) ? std::get<0>(insertions[idx]) : UINT_E_MAX;
      return vtx_id;
    });

    // Map over the vertices being modified. Overwrite their incident edges with
    // (level_id, neighbor_id) pairs, sort by level_id, resize each level to the
    // correct size, and then insert in parallel.
    parallel_for(0, starts.size() - 1, [&] (size_t i) {
      size_t idx = starts[i];
      uintE vtx = std::get<0>(insertions[idx]);
      uintE our_level = L[vtx].level;
      uintE incoming_degree = starts[i+1] - starts[i];
      auto neighbors = parlay::make_slice(insertions.begin() + idx, insertions.begin() + idx + incoming_degree);

      // Map the incident edges to (level, neighbor_id).
      parallel_for(0, incoming_degree, [&] (size_t off) {
        auto [u, v] = neighbors[off];
        assert(vtx == u);
        uintE neighbor_level = L[v].level;
        if (neighbor_level >= our_level) { neighbor_level = kUpLevel; }
        neighbors[off] = {neighbor_level, v};
      });

      // Sort neighbors by level.
      parlay::sort_inplace(neighbors);

      // Resize level containers. Can do this in parallel in many ways (e.g.,
      // pack), but a simple way that avoids memory allocation is to map over
      // everyone, and have the first index for each level search for the level.
      // If we use a linear search the algorithm is work eff. and has depth
      // proportional to the max incoming size of a level. We can also improve
      // to log(n) depth by using a doubling search.
      parallel_for(0, neighbors.size(), [&] (size_t i) {
        if ((i == 0) || neighbors[i].first != neighbors[i-1].first) {  // start of a new level
          uintE level_id = neighbors[i].first;
          size_t j = i;
          for (; j<neighbors.size(); j++) {
            if (neighbors[j].first != level_id) break;
          }
          uintE num_in_level = j - i;

          if (level_id != kUpLevel) {
            L[vtx].down[level_id].resize(num_in_level);
          } else {
            L[vtx].up.resize(num_in_level);
          }

        }
      });

      // Insert neighbors into the correct level incident to us.
      parallel_for(0, neighbors.size(), [&] (size_t i) {
        auto [level_id, v] = neighbors[i];
        if (level_id != kUpLevel) {
          bool inserted = L[vtx].down[level_id].insert(v);
          assert(inserted);
        } else {
          bool inserted = L[vtx].up.insert(v);
          assert(inserted);
        }
      });

    }, 10000000000);  // for testing

    // New edges are done being inserted. Update the level structure.
    // Interface: supply vertex seq -> process will settle everything.
    settle(std::move(affected));
  }

  void check_invariants() {
    bool invs_ok = true;
    for (size_t i=0; i<n; i++) {
      bool upper_ok = L[i].upper_invariant(levels_per_group);
      bool lower_ok = L[i].lower_invariant(levels_per_group);
      assert(upper_ok);
      assert(lower_ok);
      invs_ok &= upper_ok;
      invs_ok &= lower_ok;
    }
    std::cout << "invs ok is: " << invs_ok << std::endl;
  }

  inline uintE group_for_level(uintE level) const {
    return level / levels_per_group;
  }
};

template <class Graph>
inline void RunLDS(Graph& G) {
  using W = typename Graph::weight_type;
  size_t n = G.n;
  auto layers = LDS(n);

  auto insertions = parlay::sequence<LDS::edge_type>::uninitialized(100);
  for (size_t i=0; i<100; i++) {
    insertions[i] = {i, i + 1};
  }

  layers.batch_insertion(insertions);

//  for (size_t i = 0; i < n; i++) {
//    auto map_f = [&](const uintE& u, const uintE& v, const W& wgh) {
//      if (u < v) {
//        layers.insert_edge({u, v});
//      }
//    };
//    G.get_vertex(i).out_neighbors().map(map_f, /* parallel = */ false);
//  }

  std::cout << "Finished all insertions!" << std::endl;
  layers.check_invariants();

//  for (size_t i = 0; i < n; i++) {
//    auto map_f = [&](const uintE& u, const uintE& v, const W& wgh) {
//      if (u < v) {
//        layers.delete_edge({u, v});
//      }
//    };
//    G.get_vertex(i).out_neighbors().map(map_f, /* parallel = */ false);
//  }
//
//  std::cout << "Finished all deletions!" << std::endl;
//  layers.check_invariants();
//
//  size_t sum_lev = 0;
//  for (size_t i=0; i<G.n; i++) {
//    sum_lev += layers.L[i].level;
//  }
//  std::cout << "sum_lev = " << sum_lev << std::endl;
}

}  // namespace gbbs
