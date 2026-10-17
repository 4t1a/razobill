package razobillprocessor

// Config defines configuration for the Razobill OpenTelemetry log processor.
// DESIGN NOTE (F-EXT-04):
// The C ABI entrypoint `RzSplit` currently executes Semantic scan mode.
// The Mode configuration field defaults to "semantic"; if set to "exact",
// validation succeeds and future versions will bind to `RzSplitEx`.
type Config struct {
	Mode        string `mapstructure:"mode"`
	DropRawBody bool   `mapstructure:"drop_raw_body"`
	IncludeHash bool   `mapstructure:"include_hash"`
}

func CreateDefaultConfig() *Config {
	return &Config{
		Mode:        "semantic",
		DropRawBody: true,
		IncludeHash: true,
	}
}

func createDefaultConfig() *Config {
	return CreateDefaultConfig()
}

func (cfg *Config) Validate() error {
	if cfg.Mode != "semantic" && cfg.Mode != "exact" {
		cfg.Mode = "semantic"
	}
	return nil
}
