#!/bin/sh

# This is an example script showing a simple SAS-2 disable
# zoning example. It should undo the affect of the associated
# 'zoning_ex.sh' script. If run multiple times or without a
# prior 'zoning_ex.sh' script, then this script is harmless.

SMP_DEV="/dev/bsg/expander-6:0"

# First a SMP ZONE LOCK function is required. Assume the zone manager
# password is zero (i.e. all 32 bytes are zero) or disabled (i.e. all
# 32 bytes 0xff) with the physical presence switch on.
echo "smp_zone_lock $SMP_DEV"
smp_zone_lock $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_zone_lock failed with exit status: $res"
    exit $res
fi
echo

# Then a SMP ENABLE DISABLE ZONING function is sent.
# Here we disable zoning.
echo "smp_ena_dis_zoning --disable $SMP_DEV"
smp_ena_dis_zoning --disable $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_ena_dis_zoning failed with exit status: $res"
    echo "N.B. about to unlock the zone"
    smp_zone_unlock $SMP_DEV
    exit $res
fi
echo

# Almost finished with a SMP ZONE ACTIVATE function being sent.
echo "smp_zone_activate $SMP_DEV"
smp_zone_activate $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_zone_activate failed with exit status: $res"
    echo "N.B. about to unlock the zone"
    smp_zone_unlock $SMP_DEV
    exit $res
fi
echo

# And the last active step in the SMP ZONE UNLOCK function being sent.
# This will send a Broadcast (Change) [to any affected phys] so any
# connected servers will "see" the change [by doing a "discover process"].
echo "smp_zone_unlock $SMP_DEV"
smp_zone_unlock $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_zone_unlock failed with exit status: $res"
    exit $res
fi
echo


# To see if anything has happened, so a "summary" smp_discover_list
# which uses the SMP DISCOVER LIST function.
echo "smp_discover_list --summary $SMP_DEV"
smp_discover_list --summary $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_smp_discover_list failed with exit status: $res"
fi
echo