#!/bin/sh
BIN=
SBIN=
OSTEST=
LTP=
BOOTPARAM="-c 1-7,9-15,17-23,25-31 -m 10G@0,10G@1 -r 1-7:0+9-15:8+17-23:16+25-31:24"

if [ -f ../../../config.h ]; then
	str=`grep "^#define BINDIR " ../../../config.h | head -1 | sed 's/^#define BINDIR /BINDIR=/'`
	eval $str
fi
if [ "x$BINDIR" = x ];then
	BINDIR="$BIN"
fi

if [ -f ../../../Makefile ]; then
	str=`grep ^SBINDIR ../../../Makefile | head -1 | sed 's/ //g'`
	eval $str
fi
if [ "x$SBINDIR" = x ];then
	SBINDIR="$SBIN"
fi

if [ -f $HOME/ltp/testcases/bin/sched_setaffinity01 ]; then
	LTPDIR=$HOME/ltp/testcases
fi
if [ "x$LTPDIR" = x ]; then
	LTPDIR="$LTP"
fi

if [ -f $HOME/ostest/bin/test_mck ]; then
	OSTESTDIR=$HOME/ostest/
fi
if [ "x$OSTESTDIR" = x ]; then
	OSTESTDIR="$OSTEST"
fi

if [ ! -x $SBINDIR/mcstop+release.sh ]; then
	echo mcstop+releas: not found >&2
	exit 1
fi
echo -n "mcstop+release.sh ... "
sudo $SBINDIR/mcstop+release.sh
echo "done"

if [ ! -x $SBINDIR/mcreboot.sh ]; then
	echo mcreboot: not found >&2
	exit 1
fi
echo -n "mcreboot.sh $BOOTPARAM ... "
sudo $SBINDIR/mcreboot.sh $BOOTPARAM
echo "done"

if [ ! -x $BINDIR/mcexec ]; then
	echo mcexec: not found >&2
	exit 1
fi

tid=001
echo "*** RT_$tid start *******************************"
sudo $BINDIR/mcexec $OSTESTDIR/bin/test_mck -s sched_setaffinity -n 0 -- -p 20 2>&1 | tee ./RT_${tid}.txt
if grep "RESULT: ok" ./RT_${tid}.txt > /dev/null 2>&1 ; then
	echo "*** RT_$tid: PASSED"
else
	echo "*** RT_$tid: FAILED"
fi
echo ""

tid=002
echo "*** RT_$tid start *******************************"
sudo $BINDIR/mcexec $OSTESTDIR/bin/test_mck -s sched_setaffinity -n 1 -- -p 20 2>&1 | tee ./RT_${tid}.txt
if grep "RESULT: ok" ./RT_${tid}.txt > /dev/null 2>&1 ; then
	echo "*** RT_$tid: PASSED"
else
	echo "*** RT_$tid: FAILED"
fi
echo ""

tid=003
echo "*** RT_$tid start *******************************"
sudo $BINDIR/mcexec $OSTESTDIR/bin/test_mck -s sched_getaffinity -n 3 -- -p 20 2>&1 | tee ./RT_${tid}.txt
if grep "RESULT: ok" ./RT_${tid}.txt > /dev/null 2>&1 ; then
	echo "*** RT_$tid: PASSED"
else
	echo "*** RT_$tid: FAILED"
fi
echo ""

tid=004
echo "*** RT_$tid start *******************************"
sudo $BINDIR/mcexec $OSTESTDIR/bin/test_mck -s sched_getaffinity -n 5 -- -p 20 2>&1 | tee ./RT_${tid}.txt
if grep "RESULT: ok" ./RT_${tid}.txt > /dev/null 2>&1 ; then
	echo "*** RT_$tid: PASSED"
else
	echo "*** RT_$tid: FAILED"
fi
echo ""

tid=001
echo "*** LT_$tid start *******************************"
sudo $BINDIR/mcexec $LTPDIR/bin/sched_setaffinity01 2>&1 | tee ./LT_${tid}.txt
ok=`grep TPASS LT_${tid}.txt | wc -l`
ng=`grep TFAIL LT_${tid}.txt | wc -l`
if [ $ng = 0 ]; then
	echo "*** LT_$tid: PASSED (ok:$ok)"
else
	echo "*** LT_$tid: FAILED (ok:$ok, ng:$ng)"
fi
