// MRM w/ input (integer-safe version)

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <numeric>
#include <algorithm>
#include <limits>
#include <iomanip>

using namespace std;

const long long INF = numeric_limits<long long>::max();

// --- Utility Functions for Parsing ---
vector<long long> parseVector(string input) {
    replace(input.begin(), input.end(), '[', ' ');
    replace(input.begin(), input.end(), ']', ' ');
    replace(input.begin(), input.end(), ',', ' ');

    stringstream ss(input);

    long long val;
    vector<long long> result;

    while (ss >> val)
        result.push_back(val);

    return result;
}

vector<vector<long long>> parseMatrix(string input) {
    replace(input.begin(), input.end(), '[', ' ');

    if (!input.empty() && input.back() == ']')
        input.pop_back();

    vector<vector<long long>> matrix;

    stringstream ss(input);
    string rowStr;

    while (getline(ss, rowStr, ';')) {
        stringstream rowSS(rowStr);

        long long val;
        vector<long long> row;

        while (rowSS >> val)
            row.push_back(val);

        if (!row.empty())
            matrix.push_back(row);
    }

    return matrix;
}

// --- MRM Class Logic ---
class MaximumRangeMethod {
private:
    vector<vector<long long>> original_cost, cost;
    vector<long long> supply, demand;
    vector<vector<long long>> allocation;

    int m, n;

    bool dummy_row = false;
    bool dummy_col = false;

    string locked_dir = "";

    void balance() {
        long long total_s =
            accumulate(supply.begin(), supply.end(), 0LL);

        long long total_d =
            accumulate(demand.begin(), demand.end(), 0LL);

        if (total_s < total_d) {

            cost.push_back(vector<long long>(n, 0));

            supply.push_back(total_d - total_s);

            m++;
            dummy_row = true;
        }
        else if (total_s > total_d) {

            for (int i = 0; i < m; i++)
                cost[i].push_back(0);

            demand.push_back(total_s - total_d);

            n++;
            dummy_col = true;
        }

        allocation.assign(
            m,
            vector<long long>(n, 0)
        );
    }

    long long get_range(const vector<long long>& vec) {

        long long mn = INF;
        long long mx = 0;

        bool found = false;

        for (long long v : vec) {

            if (v != INF) {

                mn = min(mn, v);
                mx = max(mx, v);

                found = true;
            }
        }

        return found ? (mx - mn) : -1;
    }

public:
    MaximumRangeMethod(
        vector<vector<long long>> c,
        vector<long long> s,
        vector<long long> d
    )
        : original_cost(c),
          cost(c),
          supply(s),
          demand(d)
    {
        m = cost.size();
        n = cost[0].size();
    }

    void solve() {

        balance();

        int iter = 1;

        while (accumulate(
                   supply.begin(),
                   supply.end(),
                   0LL) > 0) {

            int sel_idx = -1;
            string sel_type = "";

            long long max_val = -1;

            bool do_rows =
                (iter == 1)
                    ? (!dummy_col)
                    : (locked_dir == "row");

            bool do_cols =
                (iter == 1)
                    ? (!dummy_row)
                    : (locked_dir == "col");

            if (do_rows) {

                for (int i = 0; i < m; i++) {

                    if (supply[i] > 0) {

                        long long r =
                            get_range(cost[i]);

                        if (r > max_val) {

                            max_val = r;
                            sel_idx = i;
                            sel_type = "row";
                        }
                    }
                }
            }

            if (do_cols) {

                for (int j = 0; j < n; j++) {

                    if (demand[j] > 0) {

                        vector<long long> col;

                        for (int i = 0; i < m; i++)
                            col.push_back(cost[i][j]);

                        long long r =
                            get_range(col);

                        if (r > max_val) {

                            max_val = r;
                            sel_idx = j;
                            sel_type = "col";
                        }
                    }
                }
            }

            if (iter == 1)
                locked_dir = sel_type;

            int tr = -1;
            int tc = -1;

            long long min_c = INF;

            if (sel_type == "row") {

                tr = sel_idx;

                for (int j = 0; j < n; j++) {

                    if (cost[tr][j] < min_c) {

                        min_c = cost[tr][j];
                        tc = j;
                    }
                }
            }
            else {

                tc = sel_idx;

                for (int i = 0; i < m; i++) {

                    if (cost[i][tc] < min_c) {

                        min_c = cost[i][tc];
                        tr = i;
                    }
                }
            }

            long long q =
                min(supply[tr], demand[tc]);

            allocation[tr][tc] = q;

            supply[tr] -= q;
            demand[tc] -= q;

            if (supply[tr] == 0) {

                for (int j = 0; j < n; j++)
                    cost[tr][j] = INF;
            }
            else {

                for (int i = 0; i < m; i++)
                    cost[i][tc] = INF;
            }

            iter++;
        }
    }

    void display() {

        long long total = 0;

        cout << "\n--- MRM Allocation Table ---\n";

        for (int i = 0; i < m; i++) {

            for (int j = 0; j < n; j++) {

                cout << setw(10)
                     << allocation[i][j]
                     << " ";

                if (allocation[i][j] > 0 &&
                    i < static_cast<int>(original_cost.size()) &&
                    j < static_cast<int>(original_cost[0].size())) {

                    total +=
                        allocation[i][j] *
                        original_cost[i][j];
                }
            }

            cout << '\n';
        }

        cout << "\nTotal Cost: "
             << total
             << '\n';
    }
};

int main() {

    string cost_in;
    string supply_in;
    string demand_in;

    cout << "Enter cost matrix [row1; row2; ...]: ";
    getline(cin, cost_in);

    cout << "Enter supply [s1, s2, ...]: ";
    getline(cin, supply_in);

    cout << "Enter demand [d1, d2, ...]: ";
    getline(cin, demand_in);

    auto matrix = parseMatrix(cost_in);
    auto s_vec = parseVector(supply_in);
    auto d_vec = parseVector(demand_in);

    MaximumRangeMethod mrm(
        matrix,
        s_vec,
        d_vec
    );

    mrm.solve();
    mrm.display();

    return 0;
}
