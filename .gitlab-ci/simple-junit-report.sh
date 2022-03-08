OUTFILE=$1
NAME=$2
MESSAGE=$3

cat >$OUTFILE <<EOF
<?xml version='1.0' encoding='utf-8'?>
<testsuites tests="1" errors="0" failures="1">
  <testsuite name="tracker" tests="1" errors="0" failures="1">
    <testcase name="$NAME" classname="tracker">
      <failure message="$MESSAGE"/>
    </testcase>
  </testsuite>
</testsuites>
EOF

# Also echo the message in stdout for good measure
echo $MESSAGE
