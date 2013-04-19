$fn=100;

rad = 1.6;
hole = 1.1;

difference() {
	//Base
	cube([10,10,6.5], center=true);
	//Axel hole
	difference() {
		cylinder(r=rad, h=10, center=true);
		translate([hole, -5, -10])
			cube([10, 10, 20]);
	}
}