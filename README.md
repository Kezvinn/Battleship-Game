# Battleship-Game
The game build on the NUC140 development board

Features of the product:
  - UART channel 0 for communication  
      -  Use CH341A to load map into NUC140
  - LCD display
      - Display 8x8 map with '-' represent water and 'X' if the shot is match with coordinate of the ship
  - 7-segment display
      - 2 digits (U13 and U14) to display count for number of shots
      - 1 digit (U11) to display the coordinate selected for X and Y 
  - 3x3 key matrix
      - 1-8 to select coordinate on the map,
      - 9 to switch from X to Y and vice versa
  - Button (external interrupt)
      - Switch stage from Welcome screen to Game
      - Confirm shot after coordinate is inputed
  - Buzzer
      - Toggle 5 times when the game is over (Win and Lose)
  - LED
      - Blink 3 times if shot is hit.

How to play:
- Player can use the keymatrix (3x3) to select the coordinate, from 1-8, and digit 9 to switch between X and Y
- After confirm the coordinate, player can press the SW_INT(GP15) to shot
- If shot success, the map will display 'X' at the shot coordinate. Otherwise, the map remain the same.
- Win condition:    - Player shots all 5 ships within 15 shots.
- Lose condition:   - Less than 5 ship is shot OR/AND total shot > 15
