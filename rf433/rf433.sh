#! /bin/sh
#
# rf433.sh          - Execute the rfrepeater command.
#
# Version: 1.0.0
#

name="rf433"
cmd=/usr/sbin/rfrepeater
test -x "$cmd" || exit 1

case "$1" in
	start)
		echo -n "Starting $name"
		start-stop-daemon --start --quiet --exec $cmd
		echo " ok"
		;;

	stop)
		echo -n "Stopping $name"
		start-stop-daemon --stop --quiet --exec $cmd
		echo " ok"
		;;

	restart)
		echo -n "Stopping $name"
		start-stop-daemon --stop --quiet --exec $cmd
		echo " ok"
		echo -n "Waiting for cmd to die off"
		for i in 1 2 3 ;
		do
			sleep 1
		done
		echo "Starting $name"
		start-stop-daemon --start --quiet --exec $cmd
		echo " ok"
		;;

	*)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0
