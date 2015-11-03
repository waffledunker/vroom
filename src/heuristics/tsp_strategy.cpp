/*
VROOM (Vehicle Routing Open-source Optimization Machine)
Copyright (C) 2015, Julien Coupey

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tsp_strategy.h"

void solve_atsp(const cl_args_t& cl_args){
  // Store timings.
  timing_t computing_times;

  // Building problem object with embedded table request.
  auto start_problem_build = std::chrono::high_resolution_clock::now();

  tsp asymmetric_tsp (cl_args);

  auto end_problem_build = std::chrono::high_resolution_clock::now();

  computing_times.matrix_loading =
    std::chrono::duration_cast<std::chrono::milliseconds>
    (end_problem_build - start_problem_build).count();

  if(cl_args.verbose){
    std::cout << "Matrix computing: "
              << computing_times.matrix_loading << " ms\n";
  }

  // Using symmetrized problem to apply heuristic.
  tsp_sym symmetrized_tsp (asymmetric_tsp.get_symmetrized_matrix());

  // Applying Christofides heuristic.
  auto start_christo = std::chrono::high_resolution_clock::now();
  std::unique_ptr<heuristic> christo_h = std::make_unique<christo_heuristic>();
  std::list<index_t> christo_sol
    = christo_h->build_solution(symmetrized_tsp);
  distance_t christo_cost = symmetrized_tsp.cost(christo_sol);

  auto end_christo = std::chrono::high_resolution_clock::now();

  computing_times.heuristic = 
    std::chrono::duration_cast<std::chrono::milliseconds>
    (end_christo - start_christo).count();

  if(cl_args.verbose){
    std::cout << "Christofides heuristic applied: "
              << computing_times.heuristic << " ms\n";
  }

  // Local search on symmetric problem.
  // Applying deterministic, fast local search to improve the current
  // solution in a small amount of time. All possible moves for the
  // different neighbourhoods are performed, stopping when reaching a
  // local minima.
  local_search sym_ls (symmetrized_tsp,
                       christo_sol,
                       cl_args.verbose);
  auto start_sym_local_search = std::chrono::high_resolution_clock::now();
  if(cl_args.verbose){
    std::cout << "Start local search on symmetric problem." << std::endl;
  }

  distance_t sym_two_opt_gain = 0;
  distance_t sym_relocate_gain = 0;
  distance_t sym_or_opt_gain = 0;

  do{
    // All possible 2-opt moves.
    auto start_two_opt = std::chrono::high_resolution_clock::now();
    sym_two_opt_gain = sym_ls.perform_all_two_opt_steps();

    if(cl_args.verbose){
      std::cout << " ("
                << (double) sym_two_opt_gain * 100 / christo_cost
                << "%)"
                << std::endl;
    }
    
    auto end_two_opt = std::chrono::high_resolution_clock::now();

    double two_opt_duration =
      std::chrono::duration_cast<std::chrono::milliseconds>
      (end_two_opt - start_two_opt).count();

    if(cl_args.verbose){
      std::cout << "Two-opt steps applied in: "
                << two_opt_duration << " ms\n";
    }

    // All relocate moves.
    auto start_relocate = std::chrono::high_resolution_clock::now();
    sym_relocate_gain = sym_ls.perform_all_relocate_steps();

    if(cl_args.verbose){
      std::cout << " ("
                << (double) sym_relocate_gain * 100 / christo_cost
                << "%)"
                << std::endl;
    }
    
    auto end_relocate = std::chrono::high_resolution_clock::now();

    double relocate_duration =
      std::chrono::duration_cast<std::chrono::milliseconds>
      (end_relocate - start_relocate).count();

    if(cl_args.verbose){
      std::cout << "Relocate steps applied in: "
                << relocate_duration << " ms\n";
    }

    // All or-opt moves.
    auto start_or_opt = std::chrono::high_resolution_clock::now();
    sym_or_opt_gain = sym_ls.perform_all_or_opt_steps();

    if(cl_args.verbose){
      std::cout << " ("
                << (double) sym_or_opt_gain * 100 / christo_cost
                << "%)"
                << std::endl;
    }
    
    auto end_or_opt = std::chrono::high_resolution_clock::now();

    double or_opt_duration =
      std::chrono::duration_cast<std::chrono::milliseconds>
      (end_or_opt - start_or_opt).count();

    if(cl_args.verbose){
      std::cout << "Or-Opt steps applied in: "
                << or_opt_duration << " ms\n";
    }
  }while((sym_two_opt_gain > 0) 
         or (sym_relocate_gain > 0) 
         or (sym_or_opt_gain > 0));

  std::list<index_t> current_sol = sym_ls.get_tour(0);

  auto end_sym_local_search = std::chrono::high_resolution_clock::now();

  auto sym_local_search_duration 
    = std::chrono::duration_cast<std::chrono::milliseconds>
    (end_sym_local_search - start_sym_local_search).count();
  if(cl_args.verbose){
    std::cout << "Symmetric local search: "
              << sym_local_search_duration << " ms\n";
  }

  auto asym_local_search_duration = 0;

  if(!asymmetric_tsp.is_symmetric()){
    // Back to the asymmetric problem, picking the best way.
    std::list<index_t> reverse_current_sol (current_sol);
    reverse_current_sol.reverse();
    distance_t direct_cost = asymmetric_tsp.cost(current_sol);
    distance_t reverse_cost = asymmetric_tsp.cost(reverse_current_sol);

    // Cost reference after symmetric local search.
    distance_t sym_ls_cost = std::min(direct_cost, reverse_cost);

    // Local search on asymmetric problem.
    local_search asym_ls (asymmetric_tsp,
                          (direct_cost <= reverse_cost) ? 
                          current_sol: reverse_current_sol,
                          cl_args.verbose);

    auto start_asym_local_search = std::chrono::high_resolution_clock::now();
    if(cl_args.verbose){
      std::cout << "Start local search on asymmetric problem." << std::endl;
    }
  
    distance_t asym_two_opt_gain = 0;
    distance_t asym_relocate_gain = 0;
    distance_t asym_or_opt_gain = 0;
  
    do{
      // All possible 2-opt moves.
      auto start_two_opt = std::chrono::high_resolution_clock::now();
      asym_two_opt_gain = asym_ls.perform_all_two_opt_steps();

      if(cl_args.verbose){
        std::cout << " ("
                  << (double) asym_two_opt_gain * 100 / sym_ls_cost
                  << "%)"
                  << std::endl;
      }
    
      auto end_two_opt = std::chrono::high_resolution_clock::now();

      double two_opt_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>
        (end_two_opt - start_two_opt).count();

      if(cl_args.verbose){
        std::cout << "Two-opt steps applied in: "
                  << two_opt_duration << " ms\n";
      }

      // All relocate moves.
      auto start_relocate = std::chrono::high_resolution_clock::now();
      asym_relocate_gain = asym_ls.perform_all_relocate_steps();

      if(cl_args.verbose){
        std::cout << " ("
                  << (double) asym_relocate_gain * 100 / sym_ls_cost
                  << "%)"
                  << std::endl;
      }
    
      auto end_relocate = std::chrono::high_resolution_clock::now();

      double relocate_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>
        (end_relocate - start_relocate).count();

      if(cl_args.verbose){
        std::cout << "Relocate steps applied in: "
                  << relocate_duration << " ms\n";
      }

      // All or-opt moves.
      auto start_or_opt = std::chrono::high_resolution_clock::now();
      asym_or_opt_gain = asym_ls.perform_all_or_opt_steps();

      if(cl_args.verbose){
        std::cout << " ("
                  << (double) asym_or_opt_gain * 100 / sym_ls_cost
                  << "%)"
                  << std::endl;
      }
    
      auto end_or_opt = std::chrono::high_resolution_clock::now();

      double or_opt_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>
        (end_or_opt - start_or_opt).count();

      if(cl_args.verbose){
        std::cout << "Or-Opt steps applied in: "
                  << or_opt_duration << " ms\n";
      }
    }while((asym_two_opt_gain > 0) 
           or (asym_relocate_gain > 0) 
           or (asym_or_opt_gain > 0));

    current_sol = asym_ls.get_tour(0);

    auto end_asym_local_search = std::chrono::high_resolution_clock::now();

    asym_local_search_duration 
      = std::chrono::duration_cast<std::chrono::milliseconds>
      (end_asym_local_search - start_asym_local_search).count();
    if(cl_args.verbose){
      std::cout << "Asymmetric local search: "
                << asym_local_search_duration << " ms\n";
    }
  }

  computing_times.local_search 
    = sym_local_search_duration + asym_local_search_duration;

  logger log (cl_args);
  log.tour_to_output(asymmetric_tsp, current_sol, computing_times);
}
