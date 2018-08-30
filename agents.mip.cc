#include <iostream>
#include <unordered_map>
#include "easyscip/easyscip.h"

using namespace std;
using namespace easyscip;

#define DEBUG 1 // set to 0 to remove prints

struct Agent {
    int relative_index;
    double x, y;

    Agent(int relative_index, double x, double y)
        : relative_index(relative_index), x(x), y(y) {};
};

struct Visit {
    int relative_index;
    int slot, pool_size;
    double x, y;

    Visit(int relative_index, int slot, int pool_size, double x, double y)
        : relative_index(relative_index), slot(slot), pool_size(pool_size), x(x), y(y) {};
};

double distance(double x0, double y0, double x1, double y1) {
    return sqrt((x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1));
}

void printData(vector<Agent> agents, vector<Visit> visits) {
    int i = 0;
    cout << "::::: Data read from file: " << endl << endl;

    for (Agent a : agents) {
        cout << "Agent " << i++ << ", relative_index: " << a.relative_index << " x: " << a.x << " y: " << a.y << endl;
    }

    i = 0;
    cout << endl;
    for (Visit v : visits) {
        cout << "Visit " << i++ << ", relative_index: " << v.relative_index << " slot: " << v.slot << " x: " << v.x << " y: " << v.y << " pool_size: " << v.pool_size << endl;
    }

    cout << endl;
}

void printHashTable(unordered_map<int, vector<Visit>> visitsBySlot) {
    cout << "::::: Visits by slot map: " << endl << endl;

    for (auto visitSlot : visitsBySlot) {
        cout << "Slot: " << visitSlot.first << endl;
        for (Visit v : visitSlot.second) {
            cout << "Visit relative_index: " << v.relative_index << " slot: " << v.slot << " x: " << v.x << " y: " << v.y << " pool_size: " << v.pool_size << endl;
        }
    }

    cout << endl;
}

int main() {
    int index = 0;
    int numAgents;
    int numVisits;
    double x, y;
    int slot, pool_size;

    unordered_map<int, vector<Visit>> visitsBySlot;

    cin >> numAgents;
    vector<Agent> agents;
    agents.reserve(numAgents);

    for (int i = 0; i < numAgents; ++i) {
        cin >> x >> y;
        agents.push_back(Agent(index++, x, y));
    }

    cin >> numVisits;
    vector<Visit> visits;
    visits.reserve(numVisits);

    for (int i = 0; i < numVisits; ++i) {
        cin >> slot >> x >> y >> pool_size;
        Visit v = Visit(index++, slot, pool_size, x, y);

        visits.push_back(v);

        // Build visits by slot map helper
        auto exists = visitsBySlot.find(v.slot);
        if (exists == visitsBySlot.end()) {
            visitsBySlot.insert({ v.slot, vector<Visit>{v} });
        } else {
            exists->second.push_back(v);
        }
    }

    if(DEBUG) { printData(agents, visits); }

    if(DEBUG) { printHashTable(visitsBySlot); }

    MIPSolver mip;

    // 3d solver va[a][o][d] = binary variable which means "Agent a goes from Origin o to Destination d"
    // can be used for visits and agents houses so
    // va[a1][h1][v1] = "Agent a1 went from house1 to visit1"
    // va[a1][v1][v2] = "Agent a1 went from visit1 to visit2"
    // va[a1][v2][h1] = "Agent a1 went from visit2 to house1"
    // etc ...
    // Keep in min:
    //     if we have 2 agents then:
    //        va[-][-][0] and va[-][-][1] -> destinations are the agents houses
    //        va[-][-][2] -> destination is first visit
    //     this means the first values will represent agents houses (both in origin and destination)
    //     so we will use "relative_index" from our objects to deal with that

    int totalNumPlaces = numAgents + numVisits; // numAgents == number of agents houses
    vector<vector<vector<Variable>>> va(numAgents, vector<vector<Variable>>(totalNumPlaces));

    // reserve our 3 dimension
    for (int i = 0; i < numAgents; ++i) {
        for (int j = 0; j < totalNumPlaces; ++j) {
            va[i][j].reserve(totalNumPlaces);
        }
    }

    for (Agent a : agents) {
        int agent = a.relative_index;

        // first for origin = houses
        for (Agent h : agents) {
            int origin = h.relative_index;
            for (Visit vd : visits) {
                int destination = vd.relative_index;
                if (agent != origin) { // agent always has to start from his house, if it's different then -> null
                    va[agent][origin][destination] = NullVariable();
                } else {
                    va[agent][origin][destination] = mip.binary_variable(distance(h.x, h.y, vd.x, vd.y));
                }
            }
        }

        // for destination = house
        for (Agent h : agents) {
            int destination = h.relative_index;
            for (Visit vo : visits) {
                int origin = vo.relative_index;
                if (agent != destination) { // agent always has to end at his house, if it's different then -> null
                    va[agent][origin][destination] = NullVariable();
                } else {
                    va[agent][origin][destination] = mip.binary_variable(distance(vo.x, vo.y, h.x, h.y));
                }
            }
        }

        // between visits
        for (Visit vo : visits) {
            int origin = vo.relative_index;
            for (Visit vd : visits) {
                int destination = vd.relative_index;

                // visit origin after/same slot as visit destination => no need to consider
                // (this also resolves when origin == destination, because they have the same slot)
                if (vo.slot >= vd.slot) {
                    va[agent][origin][destination] = NullVariable();
                } else {
                    va[agent][origin][destination] = mip.binary_variable(distance(vo.x, vo.y, vd.x, vd.y));
                }
            }
        }
    }

    // Restriction considering visits as destination
    // visits will either not happen (canceled) or at most one agent will participate
    for (Visit vd : visits) {
        int destination = vd.relative_index;
        auto cons = mip.constraint();

        for (Agent a : agents) {
            int agent = a.relative_index;
            cons.add_variable(va[agent][agent][destination], 1); // case when origin = agent house, already add
            for (Visit vo : visits) {
                int origin = vo.relative_index;
                if (vo.slot < vd.slot) { // only consider if destination is after origin (origin == destination -> slot == slot, so this also guards against it)
                    cons.add_variable(va[agent][origin][destination], 1);
                }
            }
        }
        cons.commit(0, 1);
    }

    // Restriction considering "continuity"
    // If an agent has visit V as destination he MUST have a visit with V as origin
    for (Agent a : agents) {
        int agent = a.relative_index;

        for (Visit vd : visits) {
            int destination = vd.relative_index;
            auto cons = mip.constraint();

            // possible origins
            for (Visit vo : visits) {
                int possible_origin = vo.relative_index;
                if (vo.slot < vd.slot) {
                    cons.add_variable(va[agent][possible_origin][destination], 1);
                }
            }

            // possible origin can also be the agent house! (agent 0 -> house 0)
            cons.add_variable(va[agent][agent][destination], 1);

            // possible destination considering visit
            for (Visit vd2 : visits) {
                int possible_destination = vd2.relative_index;
                if (vd2.slot > vd.slot) {
                    cons.add_variable(va[agent][destination][possible_destination], -1);
                }
            }

            // possible destination can also be the agent house! (agent 0 -> house 0)
            cons.add_variable(va[agent][destination][agent], -1);

            cons.commit(0, 0);
        }
    }

    // Restriction considering number of visits on each slot (cant be more than number of available agents)
    // At most cant be more than the number of agents so <= numAgents
    // At mininum it's min(totalVisitsInSlot, numAgents) because:
    //    - if in slot we have more visits than the number of agents,
    //      then the minimum visits that happen is the number of agents (7 visits, 5 agents -> 5 visits happen)
    //    - if in slot we have less visits than the number of agents,
    //      then the minimum visits that happen is the number of visits (3 visits, 5 agents -> 3 visits happen)
    for (auto visitSlot : visitsBySlot) {
        vector<Visit> visitsInSlot = visitSlot.second;
        int totalVisitsInSlot = visitsInSlot.size();

        auto cons = mip.constraint();
        for (Visit vd : visitsInSlot) {
            int destination = vd.relative_index;
            for (Agent a : agents) {
                int agent = a.relative_index;
                cons.add_variable(va[agent][agent][destination], 1); // case when origin = agent house, already add
                for (Visit vo : visits) {
                    int origin = vo.relative_index;
                    if (vo.slot < vd.slot) { // only consider if destination is after origin (origin == destination -> slot == slot, so this also guards against it)
                        cons.add_variable(va[agent][origin][destination], 1);
                    }
                }
            }
        }
        cons.commit(min(totalVisitsInSlot, numAgents), numAgents);
    }

    // Restriction because we can only have less than 5% visits "canceled"
    //     -> that will have va[agent][origin][destination] = 0
    // here I describe it the oposite way, if only less than 5% can be canceled
    // then we know the ones that will happen are between 95% and 100%
    int totalNumVisitors = 0;
    for (Visit v : visits) { totalNumVisitors += v.pool_size; }
    int numVisitors95 = totalNumVisitors * 0.95;

    if(DEBUG) { cout << endl << "::::: Number of visits (visitors) that must happen are between " << numVisitors95 << " and " << totalNumVisitors << endl << endl; }

    auto cons = mip.constraint();
    for (Visit vd : visits) {
        int destination = vd.relative_index;
        int visitors = vd.pool_size;

        for (Agent a : agents) {
            int agent = a.relative_index;
            cons.add_variable(va[agent][agent][destination], visitors); // case when origin = agent house, already add
            for (Visit vo : visits) {
                int origin = vo.relative_index;
                if (vo.slot < vd.slot) { // only consider if destination is after origin (origin == destination -> slot == slot, so this also guards against it)
                    cons.add_variable(va[agent][origin][destination], visitors);
                }
            }
        }
    }
    cons.commit(numVisitors95, totalNumVisitors);

    // Restrictions about how every agent MUST start home
    // this means that for sure
    //     va[a1][h1][v1] = "Agent a1 went from house1 to visit1"
    // this must be true fixing [a1][h1] for exactly one visit as destination
    for (Agent a : agents) {
        int agent = a.relative_index;
        int origin = agent; // remember, agent 0 -> his house 0

        auto cons = mip.constraint();
        for (Visit v : visits) {
            int destination = v.relative_index;
            if (origin == destination) {
                continue;
            }
            cons.add_variable(va[agent][origin][destination], 1);
        }
        cons.commit(0, 1); // can be 0 -> agent never left (no need, less visits than agents)
    }

    // Restrictions about how every agent MUST end at home
    // this means that for sure
    //     va[a1][v2][h1] = "Agent a1 went from visit2 to house1"
    // this must be true fixing [a1][...][h1] for exactly one visit as origin
    //
    // this is really similar to the previous restriction but
    // pay attention to both "origin" and "destination" values here!
    for (Agent a : agents) {
        int agent = a.relative_index;
        int destination = agent; // remember, agent 0 -> his house is 0

        auto cons = mip.constraint();
        for (Visit v : visits) {
            int origin = v.relative_index;
            if (origin == destination) {
                continue;
            }
            cons.add_variable(va[agent][origin][destination], 1);
        }
        cons.commit(0, 1); // can be 0 -> agent never left (no need, less visits than agents)
    }

    // Solve and print
    mip.set_time_limit(300); // 5 minutes
    auto sol = mip.solve();

    cout << endl;
    for (int agent = 0; agent < numAgents; ++agent) {
        cout << "Agent " << agent << " (House " << agent << ")" << endl;
        for (int origin = 0; origin < totalNumPlaces; ++origin) {
            for (int destination = 0; destination < totalNumPlaces; ++destination) {
                if (sol.value(va[agent][origin][destination]) > 0.5) {
                    cout << "Went from ";
                    if (origin < numAgents) {
                        cout << "House " << origin;
                    } else {
                        cout << "Visit " << origin - numAgents;
                    }
                    cout << " to ";
                    if (destination < numAgents) {
                        cout << "House " << destination;
                    } else {
                        cout << "Visit " << destination - numAgents;
                    }
                    cout << endl;
                }
            }
        }
        cout << endl;
    }

}
