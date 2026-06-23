# Lab 7: Distributed TSP

This mission implements a two-vehicle distributed traveling-salesperson task with the stock MOOS-IvP applications available in this workspace. The rescue/TSP specialty binaries referenced by the course page are not installed here, so the mission uses deterministic partitioning in `launch.sh` and standard `BHV_Waypoint` routing.

## Completed Questions

1. **Two-vehicle mission setup:** `gilda` starts at `(0,0)` and `henry` starts at `(80,0)`. Both vehicles are launched with their own MOOS communities and share with shoreside through `uFldNodeBroker`/`uFldShoreBroker`.
2. **Distributed point assignment:** the visit set is split into two route partitions. Gilda receives `20,-25:45,-60:65,-105:95,-85`; Henry receives `105,-25:130,-55:155,-95:185,-65`. Each partition is already ordered as a nearest-neighbor TSP route from that vehicle's start position.
3. **Behavior logic:** vehicles start in loiter. The DEPLOY button clears loiter/return/station flags and activates each vehicle's assigned `tsp_route` waypoint behavior. RETURN sends both vehicles back to their launch points. STATION holds position, and LOITER returns them to their loiter polygons.
4. **Visualization:** `uTimerScript` posts all eight visit points to `pMarineViewer` at startup. Gilda's points are blue and Henry's points are green.
5. **Completion behavior:** when a vehicle finishes its route, it posts `TSP_DONE_<vehicle>=true` and transitions to station-keeping.

## Run

```bash
cd ~/moos-ivp-extend/missions/lab_07/distributed_tsp
./launch.sh --just_build
./launch.sh 5
```

Use the `DEPLOY`, `RETURN`, `STATION`, and `LOITER` pMarineViewer buttons to control the mission.

## Debug Notes

The original local mission stayed in loiter because `DEPLOY` was initialized true while `LOITER` was also true, and the viewer DEPLOY button did not clear `LOITER`. That has been fixed by starting with `DEPLOY=false`, making `DEPLOY` clear the other mode flags, and adding a dedicated `TRANSITING` mode for the TSP route.
