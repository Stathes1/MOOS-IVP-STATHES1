# Lab 8: Search Grid

This mission implements a two-vehicle search-grid coverage task with the stock MOOS-IvP applications available in this workspace. It follows the same structure as the lab 7 mission: two vehicle communities, one shoreside community, deterministic route assignment, and a README documenting what is complete.

## Completed Questions

1. **Two-vehicle mission setup:** `gilda` starts at `(0,0)` and `henry` starts at `(180,0)`. Both vehicles are launched with their own MOOS communities and share node reports with shoreside through `uFldNodeBroker`/`uFldShoreBroker`.
2. **Search-grid tracking:** shoreside runs `pSearchGrid` over the polygon `{-10,0:190,0:190,-140:-10,-140}` with 10-meter cells. Each incoming `NODE_REPORT` increments the `x` value for the cell containing the vehicle.
3. **Distributed coverage assignment:** Gilda sweeps the west half of the grid and Henry sweeps the east half. The route partitions are set in `launch.sh` as lawnmower waypoint lists.
4. **Behavior logic:** vehicles start in loiter. The DEPLOY button clears loiter/return/station flags and activates each vehicle's assigned `search_sweep` waypoint behavior. RETURN sends both vehicles back to their launch points. STATION holds position, and LOITER returns them to their loiter polygons.
5. **Visualization:** `pSearchGrid` publishes `VIEW_GRID` and `VIEW_GRID_DELTA` for `pMarineViewer`. `uTimerScript` also posts the search-region outline and start markers.
6. **Completion behavior:** when a vehicle finishes its sweep, it posts `SEARCH_DONE_<vehicle>=true` and transitions to station-keeping.

## Run

```bash
cd ~/moos-ivp-extend/missions/lab_08/search_grid
./launch.sh --just_build
./launch.sh 5
```

Use the `DEPLOY`, `RETURN`, `STATION`, and `LOITER` pMarineViewer buttons to control the mission.

## Debug Notes

The mission uses `pSearchGrid`, which is available in the local MOOS-IvP tree. If the grid does not appear immediately, deploy the vehicles and watch for `VIEW_GRID`/`VIEW_GRID_DELTA` in the shoreside appcast or scope panel.
