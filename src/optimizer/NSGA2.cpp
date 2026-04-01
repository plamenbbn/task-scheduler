#include "moo/optimizer/NSGA2.hpp"

#include <algorithm>
#include <limits>
#include <numeric>

namespace moo {

namespace {

bool isFeasible(const CandidateSolution& candidate) {
    return candidate.cpuViolation <= 1e-9 && candidate.memoryViolation <= 1e-9;
}

double totalViolation(const CandidateSolution& candidate) {
    return candidate.cpuViolation + candidate.memoryViolation;
}

bool dominates(const CandidateSolution& a, const CandidateSolution& b) {
    const bool aFeasible = isFeasible(a);
    const bool bFeasible = isFeasible(b);

    if (aFeasible != bFeasible) {
        return aFeasible;
    }

    if (!aFeasible && !bFeasible) {
        return totalViolation(a) + 1e-9 < totalViolation(b);
    }

    const bool betterBenefit = a.effectiveBenefit >= b.effectiveBenefit;
    const bool betterCpu = a.objectives.cpuCost <= b.objectives.cpuCost;
    const bool betterMemory = a.objectives.memoryCost <= b.objectives.memoryCost;
    const bool betterNetwork = a.objectives.networkCost <= b.objectives.networkCost;

    const bool strictlyBetter = (a.effectiveBenefit > b.effectiveBenefit) ||
                                (a.objectives.cpuCost < b.objectives.cpuCost) ||
                                (a.objectives.memoryCost < b.objectives.memoryCost) ||
                                (a.objectives.networkCost < b.objectives.networkCost);

    return betterBenefit && betterCpu && betterMemory && betterNetwork && strictlyBetter;
}

double weightedBenefit(const TaskSnapshot& task, std::chrono::milliseconds planningWindow) {
    const auto durationMs = std::max(1.0, static_cast<double>(task.duration.count()));
    if (task.mode == ExecutionMode::OneShot) {
        return task.missionBenefit;
    }

    const auto frequencyMs = std::max(1.0, static_cast<double>(task.daemonFrequency.count()));
    const auto windowMs = std::max(1.0, static_cast<double>(planningWindow.count()));
    const double invocationCount = std::max(1.0, std::floor(windowMs / frequencyMs));
    const double dutyCycle = std::min(1.0, durationMs / frequencyMs);
    return task.missionBenefit * invocationCount * (1.0 + 0.35 * dutyCycle);
}

}  // namespace

NSGA2::NSGA2(std::size_t populationSize,
             std::size_t generations,
             double crossoverProbability,
             double mutationProbability)
    : NSGA2(populationSize,
            generations,
            crossoverProbability,
            mutationProbability,
            Limits{}) {}

NSGA2::NSGA2(std::size_t populationSize,
             std::size_t generations,
             double crossoverProbability,
             double mutationProbability,
             Limits limits)
    : populationSize_(populationSize),
      generations_(generations),
      crossoverProbability_(crossoverProbability),
      mutationProbability_(mutationProbability),
      limits_(limits) {}

std::vector<CandidateSolution> NSGA2::optimize(const std::vector<TaskSnapshot>& tasks,
                                               std::mt19937_64& rng) const {
    if (tasks.empty() || populationSize_ == 0) {
        return {};
    }

    std::vector<CandidateSolution> population;
    population.reserve(populationSize_);
    for (std::size_t i = 0; i < populationSize_; ++i) {
        auto candidate = makeRandomCandidate(tasks, rng);
        evaluateCandidate(candidate, tasks);
        population.push_back(std::move(candidate));
    }

    auto assignRanks = [this](std::vector<CandidateSolution>& pop) {
        auto fronts = fastNonDominatedSort(pop);
        for (std::size_t frontIdx = 0; frontIdx < fronts.size(); ++frontIdx) {
            const auto distances = crowdingDistances(pop, fronts[frontIdx]);
            for (std::size_t i = 0; i < fronts[frontIdx].size(); ++i) {
                const auto idx = fronts[frontIdx][i];
                pop[idx].rank = frontIdx;
                pop[idx].crowdingDistance = distances[i];
            }
        }
    };

    assignRanks(population);

    for (std::size_t generation = 0; generation < generations_; ++generation) {
        std::vector<CandidateSolution> offspring;
        offspring.reserve(populationSize_);

        while (offspring.size() < populationSize_) {
            const auto parentA = tournamentSelect(population, rng);
            const auto parentB = tournamentSelect(population, rng);
            auto children = crossover(parentA, parentB, rng);
            mutate(children.first, rng);
            mutate(children.second, rng);
            evaluateCandidate(children.first, tasks);
            evaluateCandidate(children.second, tasks);
            offspring.push_back(std::move(children.first));
            if (offspring.size() < populationSize_) {
                offspring.push_back(std::move(children.second));
            }
        }

        std::vector<CandidateSolution> combined;
        combined.reserve(population.size() + offspring.size());
        combined.insert(combined.end(), population.begin(), population.end());
        combined.insert(combined.end(), offspring.begin(), offspring.end());

        auto fronts = fastNonDominatedSort(combined);
        std::vector<CandidateSolution> next;
        next.reserve(populationSize_);

        for (std::size_t frontIdx = 0; frontIdx < fronts.size(); ++frontIdx) {
            auto distances = crowdingDistances(combined, fronts[frontIdx]);
            for (std::size_t i = 0; i < fronts[frontIdx].size(); ++i) {
                const auto idx = fronts[frontIdx][i];
                combined[idx].rank = frontIdx;
                combined[idx].crowdingDistance = distances[i];
            }

            if (next.size() + fronts[frontIdx].size() <= populationSize_) {
                for (auto idx : fronts[frontIdx]) {
                    next.push_back(combined[idx]);
                }
            } else {
                auto sortedFront = fronts[frontIdx];
                std::sort(sortedFront.begin(), sortedFront.end(), [&combined](std::size_t lhs, std::size_t rhs) {
                    return combined[lhs].crowdingDistance > combined[rhs].crowdingDistance;
                });

                const auto remaining = populationSize_ - next.size();
                for (std::size_t i = 0; i < remaining && i < sortedFront.size(); ++i) {
                    next.push_back(combined[sortedFront[i]]);
                }
                break;
            }
        }

        population = std::move(next);
        assignRanks(population);
    }

    return population;
}

CandidateSolution NSGA2::makeRandomCandidate(const std::vector<TaskSnapshot>& tasks,
                                             std::mt19937_64& rng) const {
    std::bernoulli_distribution includeDist(0.5);
    CandidateSolution candidate;
    candidate.decision.genes.resize(tasks.size());
    bool anyIncluded = false;
    for (std::size_t i = 0; i < tasks.size(); ++i) {
        candidate.decision.genes[i] = includeDist(rng) ? 1 : 0;
        anyIncluded = anyIncluded || (candidate.decision.genes[i] == 1);
    }

    if (!anyIncluded) {
        std::uniform_int_distribution<std::size_t> pick(0, tasks.size() - 1);
        candidate.decision.genes[pick(rng)] = 1;
    }

    return candidate;
}

void NSGA2::evaluateCandidate(CandidateSolution& candidate,
                              const std::vector<TaskSnapshot>& tasks) const {
    ScheduleObjective summary{};
    double effectiveBenefit = 0.0;

    for (std::size_t i = 0; i < candidate.decision.genes.size(); ++i) {
        if (candidate.decision.genes[i] == 0) {
            continue;
        }
        summary.missionBenefit += tasks[i].missionBenefit;
        summary.cpuCost += tasks[i].costs.cpu;
        summary.memoryCost += tasks[i].costs.memory;
        summary.networkCost += tasks[i].costs.network;
        effectiveBenefit += weightedBenefit(tasks[i], limits_.runFor);
    }

    candidate.objectives = summary;
    candidate.effectiveBenefit = effectiveBenefit;
    candidate.cpuViolation = std::max(0.0, summary.cpuCost - limits_.maxCpu);
    candidate.memoryViolation = std::max(0.0, summary.memoryCost - limits_.maxMemory);
}

std::vector<std::vector<std::size_t>> NSGA2::fastNonDominatedSort(const std::vector<CandidateSolution>& population) const {
    const std::size_t size = population.size();
    std::vector<std::vector<std::size_t>> fronts(1);
    std::vector<std::vector<std::size_t>> dominationSets(size);
    std::vector<std::size_t> dominationCounts(size, 0);

    for (std::size_t p = 0; p < size; ++p) {
        for (std::size_t q = 0; q < size; ++q) {
            if (p == q) {
                continue;
            }
            if (dominates(population[p], population[q])) {
                dominationSets[p].push_back(q);
            } else if (dominates(population[q], population[p])) {
                dominationCounts[p] += 1;
            }
        }
        if (dominationCounts[p] == 0) {
            fronts[0].push_back(p);
        }
    }

    std::size_t frontIdx = 0;
    while (!fronts[frontIdx].empty()) {
        std::vector<std::size_t> nextFront;
        for (std::size_t p : fronts[frontIdx]) {
            for (std::size_t q : dominationSets[p]) {
                if (dominationCounts[q] > 0) {
                    dominationCounts[q] -= 1;
                    if (dominationCounts[q] == 0) {
                        nextFront.push_back(q);
                    }
                }
            }
        }
        ++frontIdx;
        if (!nextFront.empty()) {
            fronts.push_back(nextFront);
        } else {
            fronts.push_back({});
        }
    }

    if (!fronts.empty() && fronts.back().empty()) {
        fronts.pop_back();
    }

    return fronts;
}

std::vector<double> NSGA2::crowdingDistances(const std::vector<CandidateSolution>& population,
                                             const std::vector<std::size_t>& front) const {
    const std::size_t size = front.size();
    if (size == 0) {
        return {};
    }

    std::vector<double> distances(size, 0.0);

    auto accumulateObjective = [&](auto accessor) {
        std::vector<std::size_t> indices(size);
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(), [&](std::size_t lhs, std::size_t rhs) {
            return accessor(population[front[lhs]]) < accessor(population[front[rhs]]);
        });

        distances[indices.front()] = std::numeric_limits<double>::infinity();
        distances[indices.back()] = std::numeric_limits<double>::infinity();

        const double minValue = accessor(population[front[indices.front()]]);
        const double maxValue = accessor(population[front[indices.back()]]);
        const double range = maxValue - minValue;

        if (range == 0.0) {
            return;
        }

        for (std::size_t i = 1; i + 1 < size; ++i) {
            const double nextValue = accessor(population[front[indices[i + 1]]]);
            const double prevValue = accessor(population[front[indices[i - 1]]]);
            distances[indices[i]] += (nextValue - prevValue) / range;
        }
    };

    accumulateObjective([](const CandidateSolution& c) { return -c.effectiveBenefit; });
    accumulateObjective([](const CandidateSolution& c) { return c.objectives.cpuCost; });
    accumulateObjective([](const CandidateSolution& c) { return c.objectives.memoryCost; });
    accumulateObjective([](const CandidateSolution& c) { return c.objectives.networkCost; });

    return distances;
}

CandidateSolution NSGA2::tournamentSelect(const std::vector<CandidateSolution>& population,
                                          std::mt19937_64& rng) const {
    std::uniform_int_distribution<std::size_t> dist(0, population.size() - 1);
    const auto& a = population[dist(rng)];
    const auto& b = population[dist(rng)];

    if (a.rank < b.rank) {
        return a;
    }
    if (b.rank < a.rank) {
        return b;
    }
    return (a.crowdingDistance >= b.crowdingDistance) ? a : b;
}

std::pair<CandidateSolution, CandidateSolution> NSGA2::crossover(const CandidateSolution& parentA,
                                                                 const CandidateSolution& parentB,
                                                                 std::mt19937_64& rng) const {
    std::uniform_real_distribution<double> prob(0.0, 1.0);
    if (prob(rng) > crossoverProbability_ || parentA.decision.genes.size() < 2) {
        return {parentA, parentB};
    }

    std::uniform_int_distribution<std::size_t> pointDist(1, parentA.decision.genes.size() - 1);
    const auto point = pointDist(rng);

    CandidateSolution childA = parentA;
    CandidateSolution childB = parentB;

    for (std::size_t i = point; i < parentA.decision.genes.size(); ++i) {
        std::swap(childA.decision.genes[i], childB.decision.genes[i]);
    }

    return {childA, childB};
}

void NSGA2::mutate(CandidateSolution& candidate, std::mt19937_64& rng) const {
    std::uniform_real_distribution<double> prob(0.0, 1.0);
    for (auto& gene : candidate.decision.genes) {
        if (prob(rng) < mutationProbability_) {
            gene = static_cast<uint8_t>(1 - gene);
        }
    }

    if (std::none_of(candidate.decision.genes.begin(), candidate.decision.genes.end(), [](auto gene) { return gene == 1; })) {
        std::uniform_int_distribution<std::size_t> pick(0, candidate.decision.genes.size() - 1);
        candidate.decision.genes[pick(rng)] = 1;
    }
}

}  // namespace moo
