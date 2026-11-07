# No Auto Reset to Track

The game resets a car to the track by itself when it stays off the track. The database rule `allowAIAutoReset` is already off in Grand Prix and career, but the human-controlled cars are reset anyway. This addon makes the game's reset update function (`Game_AutoResetToTrack_StartTimer`) return at once, so no reset timer starts. One in-game switch; off restores the original code. Beta, tested in Grand Prix races only. MIT licence.
