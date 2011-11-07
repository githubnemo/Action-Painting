$iopencv = "-I/usr/local/include/opencv"
$lopencv = "-lopencv_core"
$limgproc = "-lopencv_imgproc"
$lhighgui = "-lopencv_highgui"


task :effect do

	Kernel.exec ("g++ misc/effect.cpp -g #{$iopencv} #{$lopencv} #{$lhighgui} #{$limgproc} -o bin/effect")
	$?.to_i
	#Kernel.exit($?.to_i)

end
