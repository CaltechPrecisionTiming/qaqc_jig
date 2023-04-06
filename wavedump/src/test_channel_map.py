for channel_map in [0,1]:
    for channel in range(16):
        if channel_map == 0:
            # We're reading out the first half of the module.
            if channel <= 7:
                new_channel = channel
            else:
                new_channel = 7 - (channel % 8) + 16
        elif channel_map == 1:
            # We're reading out the second half of the module.
            if channel <= 7:
                new_channel = channel+8
            else:
                new_channel = 7 - (channel % 8) + 24
        print("%i %i -> %i" % (channel_map,channel,new_channel))

