# Simulation used to choose the remote-tank blend time (client/CPlayer.cpp): visible jump size versus network jitter.
# Usage: python3 tools/smoothing_sim.py
import random, math, statistics
SPEED=0.38  # units/ms

def sim(mode, tau, delay_mean=40, jitter=20, loss_burst=False, seed=1, fps=60, T=20000, extrap=True, rtt_est=None):
    rnd=random.Random(seed)
    # sender: straight line at constant speed, sends every 150ms (+ stops/starts every 3s)
    def truth(t):
        # moves 2s, stops 1s
        cyc=t%3000
        base=(t//3000)*2000*SPEED
        return base+min(cyc,2000)*SPEED, (1 if cyc<2000 else 0)
    # arrivals
    arrivals=[]
    last=0
    t=0
    while t<T:
        x,mv=truth(t)
        d=max(5,rnd.gauss(delay_mean,jitter))
        arr=max(t+d,last+1)    # TCP keeps order
        last=arr
        arrivals.append((arr,x,mv,t))
        # extra sends on input change
        t+=150
    arrivals.sort()
    ai=0
    X=0; mv_r=0; err=0
    prev=None; jumps=[]; errs=[]
    dt=1000/fps
    t=0
    while t<T:
        while ai<len(arrivals) and arrivals[ai][0]<=t:
            arr,x,mv,ts=arrivals[ai]; ai+=1
            target=x
            if extrap and mv: target+=SPEED*(rtt_est if rtt_est is not None else delay_mean)
            if mode=='snap':
                X=x
            else:
                err=target-X
            mv_r=mv
        # frame
        if mode!='snap' and err!=0:
            k=min(1,dt/tau); X+=err*k; err-=err*k
        if mv_r: X+=SPEED*dt
        tx,_=truth(t)
        if prev is not None:
            ideal=SPEED*dt if mv_r else 0
            jumps.append(abs((X-prev)-ideal))
        prev=X
        errs.append(abs(X-tx))
        t+=dt
    return max(jumps), statistics.mean(jumps), statistics.mean(errs)

print("mode            tau  jitter   maxJump  meanJump  meanErr(units)")
for jit in (5,20,40):
    r=sim('snap',1,jitter=jit,extrap=False); print(f"snap           -    {jit:3d}   {r[0]:7.1f}  {r[1]:8.2f}  {r[2]:8.1f}")
    for tau in (100,200,300,500):
        r=sim('blend',tau,jitter=jit,extrap=False); print(f"blend          {tau:3d}  {jit:3d}   {r[0]:7.1f}  {r[1]:8.2f}  {r[2]:8.1f}")
    for tau in (100,200,300,500):
        r=sim('blend',tau,jitter=jit,extrap=True); print(f"blend+extrap   {tau:3d}  {jit:3d}   {r[0]:7.1f}  {r[1]:8.2f}  {r[2]:8.1f}")
