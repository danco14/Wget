#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <set>
#include <utility>

using namespace std;

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }

    vector<int> src;
    vector<int> dest;
    vector<int> cost;
    int V = 0;
    string link;

    ifstream in_file;
    in_file.open(argv[1]);

    // Parse topofile
    while(getline(in_file, link)){
      istringstream in(link);

      int num;
      in >> num;
      V = V > num ? V : num;
      src.push_back(num);
      in >> num;
      V = V > num ? V : num;
      dest.push_back(num);
      in >> num;
      cost.push_back(num);
    }

    in_file.close();

    // Create graph
    vector< vector<int> > graph(V, vector<int>(V, -999));

    // Set link values
    for(int i = 0; i < src.size(); i++){
      graph[src[i]-1][dest[i]-1] = cost[i];
      graph[dest[i]-1][src[i]-1] = cost[i];
    }

    // Compute linkstate algorithm
    ofstream out_file;
    out_file.open("output.txt");

    ifstream change_file;
    change_file.open(argv[3]);

    string msg;

    while(1){
      vector<vector<int>> dist_dict;
      vector<vector<int>> path_dict;

      //  Loop through each vertex
      for(int s = 0; s < V; s++){
        // Bellman-Ford algorithm
        vector<int> dist(V, 1000);
        vector<int> path(V, -1);

        dist[s] = 0;

        for(int k = 0; k < V-1; k++){

          for(int v = 0; v < V; v++){
            for(int u = 0; u < V; u++){
              if(graph[v][u] == -999) continue;

              if(dist[v] + graph[v][u] < dist[u]){
                dist[u] = dist[v] + graph[v][u];
                path[u] = v;
              } else if(dist[v] + graph[v][u] == dist[u]){
                int t = u;
                while(path[t] != -1){
                  if(path[t] == -1) break;
                  t = path[t];
                }

                int x = v;
                while(path[x] != -1){
                  if(path[x] == -1) break;
                  x = path[x];
                }

                if(x < t) path[u] = v;
              }
            }
          }

        }

        // Write topology entries to file
        for(int i = 0; i < V; i++){
          if(dist[i] == 1000) continue;

          out_file << i + 1;

          if(i == s) out_file << " " << s+1;

          int t = i;
          while(path[t] != -1){
            if(path[t] == s){
              out_file << " " << t+1;
              break;
            }

            t = path[t];
          }

          out_file << " " << dist[i];
          if(change_file.peek() != EOF || s+1 != V || i+1 != V) out_file << "\n";
        }

        if(change_file.peek() != EOF || s+1 < V) out_file << "\n";

        path_dict.push_back(path);
        dist_dict.push_back(dist);
      }

      ifstream msg_file;
      msg_file.open(argv[2]);

      // Send message (if applicable)
      while(getline(msg_file, msg)){
        vector<int> p;
        stringstream in(msg);
        int s, d, t;
        string m;
        in >> m; s = stoi(m);
        in >> m; d = stoi(m); t = d;

        while(path_dict[s-1][t-1] != -1){
          p.push_back(path_dict[s-1][t-1]+1);
          t = path_dict[s-1][t-1]+1;
        }

        reverse(p.begin(), p.end());

        if(p[0] == s){
          out_file << "\n" << "from " << s << " to " << d << " cost " << dist_dict[s-1][d-1] << " hops";

          for(int i = 0; i < p.size(); i++){
            out_file << " " << p[i];
          }
        } else {
          out_file << "\n" << "from " << s << " to " << d << " cost infinite hops unreachable";
        }

        out_file << " message";

        while(in >> m){
          out_file << " " << m;
        }
      }

      msg_file.close();

      // Change graph (if applicable)
      if(getline(change_file, link)){
        istringstream in(link);
        int s, d, c;
        in >> s;
        in >> d;
        in >> c;

        graph[s-1][d-1] = c;
        graph[d-1][s-1] = c;
      } else break;

      out_file << "\n\n";
    }

    out_file.close();
    change_file.close();

    return 0;
}
