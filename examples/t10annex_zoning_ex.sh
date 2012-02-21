#!/bin/sh

# This is an example script showing a SAS-2 zoning being set up 
# by calling various smp_utils utilities.

# This example is taken from the "Zone permission configuration descriptor
# examples" annex in SAS-2 (and later) drafts found at t10.org 
# The PERM_FILE is fixed to 'permf_t10annex.txt' while the mapping of
# expander phy_ids to zone groups in in the PHYINFO_FILE or can be passed on
# the command line.

if [ $1 ] ; then
if [ "-h" = $1 ] || [ "--help" = $1 ] ; then
    echo "Usage: t10annex_zoning_ex.sh [<smp_dev> [<pconf>]]"
    echo "  where:"
    echo "    <smp_dev>     expander device node"
    echo "    <pconf>       phy information file name"
    echo
    echo "t10annex_zoning_ex.sh sets up zoning on <smp_dev> according to data"
    echo "in the permf_t10annex.txt file in this directory <pconf> file. If"
    echo "either <smp_dev> or <pconf> are not given, they default to values"
    echo "coded within this script which may need editing."
    exit 0
fi
fi

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

# Note the zone permission table file is fixed. Also this file
# contains '--start=10' so the (two) descriptors are applied
# starting at zone group 10.
PERM_FILE="permf_t10annex.txt"

# the zone phy information file name is either the second argument, or
# defaults:
if [ $2 ] ; then
    PHYINFO_FILE="$2"
else
    PHYINFO_FILE="pconf_all10.txt"
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

# Then a SMP CONFIGURE ZONE PERMISSION TABLE function is sent.
# Since --save=SAV is not given, only the shadow values are updated
# (and won't be copied to the saved value during the activate step).
# Even though the expander supports 256 zone groups, it is still possible
# to provide 128 zone group style descriptors (which is the default for
# this utility). Note that smp_rep_zone_perm_tbl will output 256 style
# descriptors in this case.
echo "smp_conf_zone_perm_tbl --permf=$PERM_FILE --deduce $SMP_DEV"
smp_conf_zone_perm_tbl --permf=$PERM_FILE --deduce $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_conf_zone_perm_tbl failed with exit status: $res"
    echo "N.B. about to unlock the zone"
    smp_zone_unlock $SMP_DEV
    exit $res
fi
echo

# Next a SMP CONFIGURE ZONE PHY INFORMATION function is sent.
# Set up zone phy information descriptors and since --save=SAV is not
# given, only the shadow values are updated (and won't be copied to the
# saved value during the activate step).
echo "smp_conf_zone_phy_info --pconf=$PHYINFO_FILE $SMP_DEV"
smp_conf_zone_phy_info --pconf=$PHYINFO_FILE $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_conf_zone_phy_info failed with exit status: $res"
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


# To see if anything has happened, call smp_discover_list which uses
# the SMP DISCOVER LIST function.
echo "smp_discover_list $SMP_DEV"
smp_discover_list $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_smp_discover_list failed with exit status: $res"
fi
echo

# Do smp_discover_list again this time with the "ignore zone group"
# bit set.
echo "smp_discover_list --ignore $SMP_DEV"
smp_discover_list --ignore $SMP_DEV
res=$?
if [ $res -ne 0 ] ; then
    echo "smp_smp_discover_list failed with exit status: $res"
fi


echo
echo "The zone permission table can now be viewed with:"
echo "  smp_rep_zone_perm_tbl --bits=13 $SMP_DEV"
echo
echo "For comparison what follows is reproduced from spl2r04a.pdf Table H.4"
echo "on page 750 (formatted like the output of smp_rep_zone_perm_tbl "
echo "should be):"
echo "    0123456789012"
echo
echo "0   0100000000000"
echo "1   1111111111111"
echo "2   0100000000100"
echo "3   0100000000100"
echo "4   0100000000000"
echo "5   0100000000000"
echo "6   0100000000000"
echo "7   0100000000000"
echo "8   0100000000100"
echo "9   0100000000100"
echo "10  0111000011101"
echo "11  0100000000000"
echo "12  0100000000100"

#
# Note that the t10 annex example shows the ZP array with its origin
# (i.e. ZP[0,0]) in the top left corner. However smp_rep_zone_perm_tbl's
# output will show the origin in the top right corner (due to SCSI's big
# endian internal representation). To override that, the "--bits=13" option
# presents the output like the t10 annex example (i.e. bit oriented, little
# endian).
#
# To comply with t10 rules ZP[10,11] should be zero (as should ZP[11,10]).
