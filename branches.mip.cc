#include <iostream>
#include "easyscip/easyscip.h"

using namespace std;
using namespace easyscip;

bool valid(int i, int j, int w, int h) {
  return i >= 0 && i < w && j >= 0 && j < h;
}

int main() {
    int w,h,g; // width, height, number of groups
    cin >> w >> h >> g;

    vector<string> board(h);
    for (int i = 0; i < h; i++) {
        cin >> board[i];
    }

    // groups values and pos
    vector<int> groupValue;
    vector<int> groupi;
    vector<int> groupj;

    groupValue.reserve(g);
    groupi.reserve(g);
    groupj.reserve(g);

    // Get groups values
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (board[j][i] != '.') { // -> if not a dot then is a group, a number
                int value = board[j][i] - '0';
                groupValue.push_back(value); // char to int
                groupj.push_back(j);
                groupi.push_back(i);
            }
        }
    }

    MIPSolver mip;

    // 3d solver branches[i][j][g] = if there is a branch on board i,j considering group g
    vector<vector<vector<Variable>>> branches(h, vector<vector<Variable>>(w));
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            for (int k = 0; k < g; k++) {
                branches[j][i].push_back(mip.binary_variable(1));
            }
        }
    }

    // Restrictions considering complete board and groups
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (board[j][i] == '.') {
                auto cons = mip.constraint();
                for (int k = 0; k < g; k++) {
                    cons.add_variable(branches[j][i][k], 1);
                }
                cons.commit(1, 1);
            }
        }
    }

    // Restrictions considering total value of group
    static int dx[] = {1, -1, 0, 0};
    static int dy[] = {0, 0, 1, -1};
    for (int a = 0; a < g; a++) {
        auto cons = mip.constraint(); // for each group
        int i = groupi[a];
        int j = groupj[a];

        for (int k = 0; k < 4; k++) {
            int jj = j + dy[k], ii = i + dx[k];
            int step = 0;
            while (valid(ii, jj, w, h) && board[jj][ii] == '.' && step < groupValue[a]) {
                cons.add_variable(branches[jj][ii][a], 1);
                ii += dx[k];
                jj += dy[k];
                step++;
            }
        }

        cons.commit(groupValue[a], groupValue[a]);
    }
/*
    for (int k = 0; k < g; k++) {
        auto cons = mip.constraint(); // for each group
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                if (board[j][i] == '.') {
                    cons.add_variable(branches[j][i][k], 1);
                }
            }
        }
        cons.commit(groupValue[k], groupValue[k]);
    }
*/
    // Restrictions for rows and columns not in a "cross" for each group
    for (int k = 0; k < g; k++) {
        auto cons = mip.constraint();
        int ii = groupi[k];
        int jj = groupj[k];

        // first for rows
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                if (i != ii && j != jj) {
                    cons.add_variable(branches[j][i][k], 1);
                }
            }
        }
        cons.commit(0, 0);
    }

    // For each group we iterate line and column (until wall or another group)
    // and each step makes a new constraint

    // iterate through groups
    for (int a = 0; a < g; a++) {
        int i = groupi[a];
        int j = groupj[a];

        for (int k = 0; k < 4; k++) {
            int jj = j + dy[k], ii = i + dx[k];

            // if we are already at an impossible location, go to next k
            if (!valid(ii, jj, w, h) || board[jj][ii] != '.') {
                continue;
            }

            // each step is a constraint
            // go to the last possible path!
            int step = 1; // we already walked
            for (; step < groupValue[a]; step++) {
                jj += dy[k];
                ii += dx[k];
                if (!valid(ii, jj, w, h) || board[jj][ii] != '.') {
                    jj -= dy[k];
                    ii -= dx[k];
                    break;
                }
            }

            //cout << "I'm here! 10 - steps: " << step << " groupValue: " << groupValue[a] << endl;
            //cout << "jj: " << jj << " ii: " << ii << endl;

            // lets walk back
            while (step > 1) {
                auto cons = mip.constraint();

                int auxi = ii;
                int auxj = jj;
                int auxstep = step-1;

                cons.add_variable(branches[auxj][auxi][a], -auxstep);
                //cout << "-auxstep: " << -auxstep << " auxi " << auxi << " auxj " << auxj << endl;
                while (auxstep > 0) {
                    auxstep--;
                    auxi -= dx[k];
                    auxj -= dy[k];

                    //cout << "auxstep: " << auxstep << " auxi " << auxi << " auxj " << auxj << endl;
                    cons.add_variable(branches[auxj][auxi][a], 1);
                }
                //cout << "step-1: " << step-1 << endl;
                cons.commit(0, step-1);

                jj -= dy[k];
                ii -= dx[k];
                step--;
            }

            // one is a special case
            //if (step == 1) {
                auto cons = mip.constraint();
                cons.add_variable(branches[jj][ii][a], 1);
                cons.commit(0, 1);
            //}
        }
    }

    // Solve and print.
    auto sol = mip.solve();

    for (int k = 0; k < g; k++) {
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                if (sol.value(branches[j][i][k]) > 0.5) {
                    cout << '*';
                } else {
                    cout << board[j][i];
                }
            }
            cout << endl;
        }
        cout << endl;
    }
}
