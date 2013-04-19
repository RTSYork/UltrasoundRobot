//Ensure circles are nice and round
$fn=100;

//Variables
noOfHoles = 8;

//Calculate
angleBetweenHoles = 360 / (noOfHoles * 4);
radiusOfCircle = tan(angleBetweenHoles / 2) * 10 * 2;

//Debug
echo(str("Encoder Circle Radius: ", radiusOfCircle));

//Generate wheel
difference() {
	//Main section
	union() {
		cylinder(r=13, h=6.5, center=true);
		cylinder(r=14.25, h=3.75, center=true);
	}
	//Axel Hole
	difference() {
		cylinder(r=1.5, h=10, center=true);
		translate([0.95, -3, -10])
			cube([6,6,20]);
	}
	//Encoder cutouts
	for(i = [0 : noOfHoles - 1]) {
		rotate(a=i*4*angleBetweenHoles, v=[0,0,1])
			translate([10,0,0])
				cylinder(r=radiusOfCircle, h=10, center=true);
	}
}