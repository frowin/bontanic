Import("env")

env.AddPreAction("upload", "pio run -t uploadfs") 