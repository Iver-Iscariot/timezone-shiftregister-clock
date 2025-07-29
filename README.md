# Timezone clock
Expandable, dynamic system for showing the time in different timezones.
<img width="550" height="300" alt="image" src="https://github.com/user-attachments/assets/8386c878-1cdd-4f79-876c-e88403fdf525" />

Originally designed for use in Omega Verksted, as several members exchanged to different timezones.

The modules are chained together with one MCU at the end. The timezone for each module is set using the dip switch, and read to the MCU by shift registers. The corresponding numbers for 7-seg displays are then sent out on further registers. The dip-switch also instructs weather its the last in the chain, and if its supposed to show the minutes or the hours of the zone. The clock is design to be installed in two rows, one for hours and one for minutes


## Time zone DIP switch table
coming
