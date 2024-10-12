package razobillprocessor

/*
#cgo CFLAGS: -I../../include
#cgo LDFLAGS: -L../../build -lrazobill -lstdc++
#include "librazobill/RazobillC.h"
#include <stdlib.h>
*/
import "C"

import (
	"context"
	"unsafe"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/pdata/plog"
)

type razobillProcessor struct {
	cfg          *Config
	nextConsumer consumer.Logs
	scanMode     C.RzScanMode
}

func newRazobillProcessor(cfg *Config, next consumer.Logs) (*razobillProcessor, error) {
	var mode C.RzScanMode = C.RZ_SCAN_SEMANTIC
	if cfg.Mode == "exact" {
		mode = C.RZ_SCAN_EXACT
	}
	return &razobillProcessor{
		cfg:          cfg,
		nextConsumer: next,
		scanMode:     mode,
	}, nil
}

func (p *razobillProcessor) Start(_ context.Context, _ component.Host) error {
	return nil
}

func (p *razobillProcessor) Shutdown(_ context.Context) error {
	return nil
}

func (p *razobillProcessor) Capabilities() consumer.Capabilities {
	return consumer.Capabilities{MutatesData: true}
}

func (p *razobillProcessor) ConsumeLogs(ctx context.Context, ld plog.Logs) error {
	rls := ld.ResourceLogs()
	for i := 0; i < rls.Len(); i++ {
		sls := rls.At(i).ScopeLogs()
		for j := 0; j < sls.Len(); j++ {
			records := sls.At(j).LogRecords()
			for k := 0; k < records.Len(); k++ {
				rec := records.At(k)
				msg := rec.Body().AsString()
				if len(msg) == 0 {
					continue
				}

				var cRecord C.RzSlotRecord
				cMsg := C.CString(msg)

				status := C.RzDecompose(
					cMsg,
					C.size_t(len(msg)),
					p.scanMode,
					&cRecord,
				)

				if status == C.RZ_SUCCESS {
					formKeyStr := C.GoStringN(cRecord.formKey, C.int(cRecord.formKeyLength))
					attrs := rec.Attributes()

					if p.cfg.DropRawBody {
						rec.Body().SetStr(formKeyStr)
					} else {
						attrs.PutStr("razobill.formKey", formKeyStr)
					}

					if p.cfg.IncludeHash {
						attrs.PutInt("razobill.formHash", int64(cRecord.formKeyHash))
					}

					// Copy 64-bit integers
					if cRecord.intCount > 0 {
						intsSlice := attrs.PutEmptySlice("razobill.ints")
						intsSlice.EnsureCapacity(int(cRecord.intCount))
						intPtrs := (*[1 << 28]C.int64_t)(unsafe.Pointer(cRecord.intValues))[:cRecord.intCount:cRecord.intCount]
						for idx := 0; idx < int(cRecord.intCount); idx++ {
							intsSlice.AppendEmpty().SetInt(int64(intPtrs[idx]))
						}
					}

					// Copy variable strings
					if cRecord.strCount > 0 {
						strsSlice := attrs.PutEmptySlice("razobill.strs")
						strsSlice.EnsureCapacity(int(cRecord.strCount))
						strPtrs := (*[1 << 28]*C.char)(unsafe.Pointer(cRecord.strValues))[:cRecord.strCount:cRecord.strCount]
						strLens := (*[1 << 28]C.size_t)(unsafe.Pointer(cRecord.strLengths))[:cRecord.strCount:cRecord.strCount]
						for idx := 0; idx < int(cRecord.strCount); idx++ {
							sVal := C.GoStringN(strPtrs[idx], C.int(strLens[idx]))
							strsSlice.AppendEmpty().SetStr(sVal)
						}
					}

					C.RzFreeRecord(&cRecord)
				}
				C.free(unsafe.Pointer(cMsg))
			}
		}
	}
	return p.nextConsumer.ConsumeLogs(ctx, ld)
}
