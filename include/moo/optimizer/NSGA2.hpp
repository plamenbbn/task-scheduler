#pragma once

#include "moo/Schedule.hpp"
#include "moo/Task.hpp"
#include "moo/Types.hpp"

#include <chrono>
#include <cstddef>
#include <random>
#include <vector>

namespace moo {

struct TaskSnapshot {
    TaskId id;
    double missionBenefit;
    TaskCosts costs;
    ExecutionMode mode;
    std::chrono::milliseconds duration;
    std::chrono::milliseconds daemonFrequency;
};

struct DecisionVector {
    std::vector<uint8_t> genes;  // 0 = exclude, 1 = include (daemons handled separately later)
};

struct CandidateSolution {
    DecisionVector decision;
    ScheduleObjective objectives;
    double effectiveBenefit = 0.0;
    double cpuViolation = 0.0;
    double memoryViolation = 0.0;
    std::size_t rank = 0;
    double crowdingDistance = 0.0;
};

class NSGA2 {
public:
    struct Limits {
        double maxCpu = 100.0;
        double maxMemory = 100.0;
        std::chrono::milliseconds runFor{10'000};
    };

    NSGA2(std::size_t populationSize,
          std::size_t generations,
          double crossoverProbability,
          double mutationProbability);

    NSGA2(std::size_t populationSize,
          std::size_t generations,
          double crossoverProbability,
          double mutationProbability,
          Limits limits);

    std::vector<CandidateSolution> optimize(const std::vector<TaskSnapshot>& tasks,
                                            std::mt19937_64& rng) const;

private:
    std::size_t populationSize_;
    std::size_t generations_;
    double crossoverProbability_;
    double mutationProbability_;
    Limits limits_;

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
