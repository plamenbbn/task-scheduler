#pragma once

#include "moo/Schedule.hpp"
#include "moo/Task.hpp"
#include "moo/Types.hpp"

#include <cstddef>
#include <random>
#include <vector>

namespace moo {

struct TaskSnapshot {
    TaskId id;
    double missionBenefit;
    TaskCosts costs;
    ExecutionMode mode;
};

struct DecisionVector {
    std::vector<uint8_t> genes;  // 0 = exclude, 1 = include (daemons handled separately later)
};

struct CandidateSolution {
    DecisionVector decision;
    ScheduleObjective objectives;
    std::size_t rank = 0;
    double crowdingDistance = 0.0;
};

class NSGA2 {
public:
    NSGA2(std::size_t populationSize,
          std::size_t generations,
          double crossoverProbability,
          double mutationProbability);

    std::vector<CandidateSolution> optimize(const std::vector<TaskSnapshot>& tasks,
                                            std::mt19937_64& rng) const;

private:
    std::size_t populationSize_;
    std::size_t generations_;
    double crossoverProbability_;
    double mutationProbability_;

    CandidateSolution makeRandomCandidate(const std::vector<TaskSnapshot>& tasks,
                                          std::mt19937_64& rng) const;
    void evaluateCandidate(CandidateSolution& candidate,
                           const std::vector<TaskSnapshot>& tasks) const;

    std::vector<std::vector<std::size_t>> fastNonDominatedSort(const std::vector<CandidateSolution>& population) const;
    std::vector<double> crowdingDistances(const std::vector<CandidateSolution>& population,
                                           const std::vector<std::size_t>& front) const;
    CandidateSolution tournamentSelect(const std::vector<CandidateSolution>& population,
                                       std::mt19937_64& rng) const;
    std::pair<CandidateSolution, CandidateSolution> crossover(const CandidateSolution& parentA,
                                                             const CandidateSolution& parentB,
                                                             std::mt19937_64& rng) const;
    void mutate(CandidateSolution& candidate, std::mt19937_64& rng) const;
};

}  // namespace moo
