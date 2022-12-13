// Several stepper configuration checks
void printStepperConfig() {
  float degvirt = 360 / float(VirtSTEPS);
  float degtrue = 360 / float(STEPS);

  Serial.print("Configured to represent ");
  Serial.print(MinutesPerTick);
  Serial.print(" minutes per clock tick (");
  Serial.print(degvirt);
  Serial.println(" deg per tick)");
  Serial.print("Actual rotation per clock tick: ");
  Serial.print(degtrue*VirtStepSize);
  Serial.println(" deg");
  Serial.print("Max error per revolution: ");
  Serial.print(degtrue*StepDiff);
  Serial.println(" deg");
}
// Move through 2 rotations with 0.5 seconds between ticks
void TestStepper(){
  for (int testcounter = 0; testcounter < VirtSTEPS; testcounter++){
    Serial.println(testcounter);
    //VirtualStep(1);
  }
}
