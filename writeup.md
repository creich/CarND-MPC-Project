## MPC Model description

the state of the vehicle is modeled using a `kinematic bicicle model`. therefor i keep track of a state consisting of
* position `{x, y}`
* orientation angle `psi`
* velocity `v`

i also track two current errors. namely:
* cross track error `CTE`
* orientation error `epsi`

here are the kinematic model equations, as stated during the lessons:
* `x[t+1] = x[t] + v[t] * cos(psi[t]) * dt`
* `y[t+1] = y[t] + v[t] * sin(psi[t]) * dt`
* `psi_[t+1] = psi[t] + v[t] / Lf * delta[t] * dt`
* `v_[t+1] = v[t] + a[t] * dt`
* `cte[t+1] = f(x[t]) - y[t] + v[t] * sin(epsi[t]) * dt`
* `epsi[t+1] = psi[t] - psides[t] + v[t] * delta[t] / Lf * dt`


## chosen N (timestep length) and dt (elapsed duration between timesteps) values

i went with a very popular choice of `N=10, dt=0.1` which was already mentioned during the lessons
i did not change those values much, since they performed well. besides that i had some trouble to get this project running
on my local PC so i had to use the online workspace. that somehow works, but really gave me a huge lag and made tests with 
different setting very difficult, since i could hardly see any changes in the stuttering transmission of the simulator..

anyway, i try to discuss some possible changes in theroy and will give them a try in the future, once i get the project to
work on my local machine
* Why smaller dt is better? 
  * the smaller dt, the smaller the distance between the calculated waypoints. that would make a smoother line prediction possible
* Why larger N isn't always better?
  * the higher N, the higher the computational costs. since N defines the number of waypoints to be calucalted it obviously will
    take more resources/time to calc all of those.
' How does time horizon (N*dt) affect the predicted path?
  * a to big time horizon may 'point to far ahead' (depending on dt and the current speed) wich may not be
    useful at all. e.g. lets say we calulate that many points N that we could theoretically point 1km ahead. the chances that
    while driving in that direction some changes to the path are necessary are very high

