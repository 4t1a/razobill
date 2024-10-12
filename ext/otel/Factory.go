package razobillprocessor

import (
	"context"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/processor"
)

var typeStr = component.MustNewType("razobill")

func NewFactory() processor.Factory {
	return processor.NewFactory(
		typeStr,
		createDefaultConfigFunc,
		processor.WithLogs(createLogsProcessor, component.StabilityLevelBeta),
	)
}

func createDefaultConfigFunc() component.Config {
	return createDefaultConfig()
}

func createLogsProcessor(
	_ context.Context,
	_ processor.Settings,
	cfg component.Config,
	next consumer.Logs,
) (processor.Logs, error) {
	oCfg := cfg.(*Config)
	return newRazobillProcessor(oCfg, next)
}
