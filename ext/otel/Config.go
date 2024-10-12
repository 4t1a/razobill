package razobillprocessor

type Config struct {
	Mode        string `mapstructure:"mode"`
	DropRawBody bool   `mapstructure:"drop_raw_body"`
	IncludeHash bool   `mapstructure:"include_hash"`
}

func createDefaultConfig() *Config {
	return &Config{
		Mode:        "semantic",
		DropRawBody: true,
		IncludeHash: true,
	}
}

func (cfg *Config) Validate() error {
	if cfg.Mode != "semantic" && cfg.Mode != "exact" {
		cfg.Mode = "semantic"
	}
	return nil
}
