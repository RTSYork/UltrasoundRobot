//Ensure circles are nice and round
$fn=100;

//Generate wheel
difference() {
	//Main section
	union() {
		//Inner wheel
		cylinder(r=13, h=6.5, center=true);
		//Wheel rim
		cylinder(r=14.25, h=3.75, center=true);
	}
	//Axel hole
	difference() {
		cylinder(r=1.6, h=10, center=true);
		translate([1.1, -3, -10])
			cube([6, 6, 20]);
	}
	//Alex hole cutout
//	translate([-3.41, -0.5, -10])
//		cube([2, 1, 20]);
}