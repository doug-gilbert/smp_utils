#!/bin/sh

# This is an example script showing a simple SAS-2 zoning example
# using various smp_utils utilities.

# Assume:
#   phy_id 0: uplink to HBA on "active zone manager" machine
#   phy_id 1: disk (SAS or SATA)
#   phy_id 2: disk (SAS or SATA)
#   phy_id 3: disk (SAS or SATA)
# also assume this script is executed in the same directory as
# its helper files: pconf_8.txt, permf_8all.txt and permf_8isol.txt 

# Set SMP_DEV to the first argument given to this script or the fixed
# name shown below. This identifies an expander device (using a bsg
# device node in the fixed name shown below)). Place SMP_DEV
# on the command line of each smp_utils invocation. If the fixed name
# below is used it will probably need to be changed (e.g. look at
# 'ls /dev/bsg' output for available expander nodes).
if [ $1 ] ; then
    SMP_DEV="$1"
else
    SMP_DEV="/dev/bsg/expander-6:0"
fi

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

# Next a SMP CONFIGURE ZONE PHY INFORMATION function is sent.
# Set up zone phy information descriptors and since --save=SAV is not
# given, only the shadow values are updated (and won't be copied to the
# saved value during the activate step).
echo "smp_conf_zone_phy_info --pconf=pconf_8.txt $SMP_DEV"
smp_conf_zone_phy_info --pconf=pconf_8.txt $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_conf_zone_phy_info failed with exit status: $res"
    echo "N.B. about to unlock the zone"
    smp_zone_unlock $SMP_DEV
    exit $res
fi
echo

# Then a SMP CONFIGURE ZONE PERMISSION TABLE function is sent.
# Since --save=SAV is not given, only the shadow values are updated
# (and won't be copied to the saved value during the activate step).
# Even though the expander support 256 zone groups, it is still possible
# to provide 128 zone group style descriptors (which is the default for
# this utility). Note that smp_rep_zone_perm_tbl will output 256 style
# descriptors in this case.
echo "smp_conf_zone_perm_tbl --permf=permf_8isol.txt $SMP_DEV"
smp_conf_zone_perm_tbl --permf=permf_8isol.txt $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_conf_zone_perm_tbl failed with exit status: $res"
    echo "N.B. about to unlock the zone"
    smp_zone_unlock $SMP_DEV
    exit $res
fi
echo

# Then a SMP ENABLE DISABLE ZONING function is sent.
# Enable zoning is the default action of this utility.
echo "smp_ena_dis_zoning $SMP_DEV"
smp_ena_dis_zoning $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_ena_dis_zoning failed with exit status: $res"
    echo "N.B. about to unlock the zone"
    smp_zone_unlock $SMP_DEV
    exit $res
fi
echo

# Almost finished with a SMP ZONE ACTIVATE function being sent.
# The above zone phy information, permission table and enable-disable
# values are copied from the shadow values to the current values
# making them "live". Note the "saved" values haven't been altered so
# an expander power cycle (or a disable zoning) will undo any damage
# done by this example.
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

# Do "summary" smp_discover_list again this time with the
# "ignore zone group" bit set.
echo "smp_discover_list --ignore -S $SMP_DEV"
smp_discover_list --ignore -S $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_smp_discover_list failed with exit status: $res"
fi

