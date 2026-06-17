#!/usr/bin/env bash
WARP=${1:-1}
echo "Launching s16_alpha_bravo_pshare with WARP=$WARP"
pAntler shoreside.moos >& /dev/null &
sleep 0.5
pAntler alpha.moos >& /dev/null &
sleep 0.5
pAntler bravo.moos >& /dev/null &
echo "Launched shoreside + alpha + bravo."
echo "Kill with: ktm"
