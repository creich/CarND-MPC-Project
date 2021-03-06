#include "json.hpp"
#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "MPC.h"

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
std::string hasData(std::string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.rfind("}]");
  if (found_null != std::string::npos) {
    return "";
  } else if (b1 != std::string::npos && b2 != std::string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

// Evaluate a polynomial.
double polyeval(Eigen::VectorXd coeffs, double x) {
  double result = 0.0;
  for (int i = 0; i < coeffs.size(); i++) {
    result += coeffs[i] * pow(x, i);
  }
  return result;
}

// Fit a polynomial.
// Adapted from
// https://github.com/JuliaMath/Polynomials.jl/blob/master/src/Polynomials.jl#L676-L716
Eigen::VectorXd polyfit(Eigen::VectorXd xvals, Eigen::VectorXd yvals,
                        int order) {
  assert(xvals.size() == yvals.size());
  assert(order >= 1 && order <= xvals.size() - 1);
  Eigen::MatrixXd A(xvals.size(), order + 1);

  for (int i = 0; i < xvals.size(); i++) {
    A(i, 0) = 1.0;
  }

  for (int j = 0; j < xvals.size(); j++) {
    for (int i = 0; i < order; i++) {
      A(j, i + 1) = A(j, i) * xvals(j);
    }
  }

  auto Q = A.householderQr();
  auto result = Q.solve(yvals);
  return result;
}

int main() {
  uWS::Hub h;

  // MPC is initialized here!
  MPC mpc;

  h.onMessage([&mpc](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    std::string sdata = std::string(data).substr(0, length);
    std::cout << sdata << std::endl;
    if (sdata.size() > 2 && sdata[0] == '4' && sdata[1] == '2') {
      std::string s = hasData(sdata);
      if (s != "") {
        auto j = json::parse(s);
        std::string event = j[0].get<std::string>();
        if (event == "telemetry") {
          // j[1] is the data JSON object
          // 6 waypoints from the simulator
          std::vector<double> ptsx = j[1]["ptsx"];
          std::vector<double> ptsy = j[1]["ptsy"];
          // current car data
          double px = j[1]["x"];
          double py = j[1]["y"];
          double psi = j[1]["psi"];
          double v = j[1]["speed"];

          // artificially fix the cars reference system in a way that
          //   * the car's angle psi will be 0
          //   * the 'location' is set to origin
          // just to simplify the math (e.g. for polyfit)
          for(int i=0; i<ptsx.size(); i++) {
             double shift_x = ptsx[i] - px;
             double shift_y = ptsy[i] - py;

             ptsx[i] = shift_x * cos(0-psi) - shift_y * sin(0-psi);
             ptsy[i] = shift_x * sin(0-psi) + shift_y * cos(0-psi);
          }

          // put waypoints into Eigen::vectors to feed them to polyfit
          // found a way to transform the vectors easily here: 
          //   https://forum.kde.org/viewtopic.php?f=74&t=94839
          //   https://eigen.tuxfamily.org/dox/classEigen_1_1Map.html
          //   Eigen::VectorXd ptsx_eigen = Eigen::VectorXd::Map(ptsx.data(), ptsx.size());
          Eigen::Map<Eigen::VectorXd> ptsx_eigen(ptsx.data(), ptsx.size());
          Eigen::Map<Eigen::VectorXd> ptsy_eigen(ptsy.data(), ptsy.size());

          Eigen::VectorXd coeffs = polyfit(ptsx_eigen, ptsy_eigen, 3);

          // calculate CTE and ePSI
          double cte = polyeval(coeffs, 0);
          //TODO check if we wanne use the full implementation of ePSI
          // correct implementation of ePSI:
          //double epsi = psi - atan( coeffs[1] + 2 * px * coeffs[2] + 3 * coeffs[3] * pow(px, 2) );
          // can be simplified due to our reference system simplification that made px == 0 and psi == 0 to
          // epsi = 0 - atan( coeffs[1] + 2 * 0 * coeffs[2] + 3 * coeffs[3] * pow(0, 2))
          // epsi = 0 - atan( coeffs[1] +         0         +          0               )
          double epsi = -atan(coeffs[1]);

          double steer_value = j[1]["steering_angle"];
          double throttle_value = j[1]["throttle"];

          Eigen::VectorXd state(6);
          //state << 0, 0, 0, v, cte, epsi;
          // Predict 0.1 second ahead to compensate latency
          double latency = 0.1;
          const double Lf = 2.67;
          state[0] = v * cos(-steer_value) * latency;
          state[1] = v * sin(-steer_value) * latency;
          state[2] = (-v * steer_value / Lf * latency);
          state[3] = v + throttle_value * latency;
          state[4] = cte + v * sin(epsi) * latency;
          state[5] = epsi - v * steer_value / Lf * latency;

          /*
          * Calculate steering angle and throttle using MPC.
          *
          * Both are in between [-1, 1].
          *
          */

          std::vector<double> vars = mpc.Solve(state, coeffs);

          // NOTE: Remember to divide by deg2rad(25) before you send the steering value back.
          // Otherwise the values will be in between [-deg2rad(25), deg2rad(25] instead of [-1, 1].
          steer_value = vars[0] / (deg2rad(25)*Lf);
          throttle_value = vars[1];

          json msgJson;
          msgJson["steering_angle"] = steer_value;
          msgJson["throttle"] = throttle_value;

          //Display the MPC predicted trajectory 
          std::vector<double> mpc_x_vals;
          std::vector<double> mpc_y_vals;

          //.. add (x,y) points to list here, points are in reference to the vehicle's coordinate system
          // the points in the simulator are connected by a Green line

          for(int i=2; i<vars.size(); i++) {
             if(i%2 == 0) {
                mpc_x_vals.push_back(vars[i]);
             } else {
                mpc_y_vals.push_back(vars[i]);
             }
          }

          msgJson["mpc_x"] = mpc_x_vals;
          msgJson["mpc_y"] = mpc_y_vals;

          //Display the waypoints/reference line
          // for better debugging we calc the line that the car will try to
          // follow and make it displayable within the simulator
          std::vector<double> next_x_vals;
          std::vector<double> next_y_vals;

          //.. add (x,y) points to list here, points are in reference to the vehicle's coordinate system
          // the points in the simulator are connected by a Yellow line
          double poly_inc = 2.0;
          int num_way_points = 23;
          for(int i=0; i<num_way_points; i++) {
             next_x_vals.push_back(poly_inc * i);
             next_y_vals.push_back(polyeval(coeffs, poly_inc * i));
          }


          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;


          auto msg = "42[\"steer\"," + msgJson.dump() + "]";
          std::cout << msg << std::endl;
          // Latency
          // The purpose is to mimic real driving conditions where
          // the car does actuate the commands instantly.
          //
          // Feel free to play around with this value but should be to drive
          // around the track with 100ms latency.
          //
          // NOTE: REMEMBER TO SET THIS TO 100 MILLISECONDS BEFORE
          // SUBMITTING.
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the
  // program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
