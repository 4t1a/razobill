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

	"github.com/cespare/xxhash/v2"
	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/pdata/plog"
)

type RazobillProcessor struct {
	cfg          *Config
	nextConsumer consumer.Logs
}

func NewRazobillProcessor(cfg *Config, next consumer.Logs) (*RazobillProcessor, error) {
	return &RazobillProcessor{
		cfg:          cfg,
		nextConsumer: next,
	}, nil
}

func newRazobillProcessor(cfg *Config, next consumer.Logs) (*RazobillProcessor, error) {
	return NewRazobillProcessor(cfg, next)
}

func (p *RazobillProcessor) Start(_ context.Context, _ component.Host) error {
	return nil
}

func (p *RazobillProcessor) Shutdown(_ context.Context) error {
	return nil
}

func (p *RazobillProcessor) Capabilities() consumer.Capabilities {
	return consumer.Capabilities{MutatesData: true}
}

func (p *RazobillProcessor) ConsumeLogs(ctx context.Context, ld plog.Logs) error {
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

				var cRecord C.RzRecord
				cMsg := C.CString(msg)

				status := C.RzSplit(
					cMsg,
					C.size_t(len(msg)),
					&cRecord,
				)

				if status == C.RZ_OK {
					tmplStr := C.GoStringN(cRecord.templateStr, C.int(cRecord.templateLength))
					attrs := rec.Attributes()

					if p.cfg.DropRawBody {
						rec.Body().SetStr(tmplStr)
					} else {
						attrs.PutStr("razobill.template", tmplStr)
					}

					if p.cfg.IncludeHash {
						attrs.PutInt("razobill.formHash", int64(xxhash.Sum64String(tmplStr)))
					}

					// Copy 64-bit integers
					if cRecord.intCount > 0 && cRecord.intValues != nil {
						intsSlice := attrs.PutEmptySlice("razobill.ints")
						intsSlice.EnsureCapacity(int(cRecord.intCount))
						intPtrs := unsafe.Slice((*int64)(unsafe.Pointer(cRecord.intValues)), int(cRecord.intCount))
						for _, val := range intPtrs {
							intsSlice.AppendEmpty().SetInt(val)
						}
					}

					// Copy variable strings
					if cRecord.strCount > 0 && cRecord.strValues != nil && cRecord.strLengths != nil {
						strsSlice := attrs.PutEmptySlice("razobill.strs")
						strsSlice.EnsureCapacity(int(cRecord.strCount))
						strPtrs := unsafe.Slice(cRecord.strValues, int(cRecord.strCount))
						strLens := unsafe.Slice(cRecord.strLengths, int(cRecord.strCount))
						for idx := 0; idx < int(cRecord.strCount); idx++ {
							sVal := C.GoStringN(strPtrs[idx], C.int(strLens[idx]))
							strsSlice.AppendEmpty().SetStr(sVal)
						}
					}

					C.RzFree(&cRecord)
				} else {
					rec.Attributes().PutStr("razobill.error", C.GoString(C.RzStatusMessage(status)))
				}
				C.free(unsafe.Pointer(cMsg))
			}
		}
	}
	return p.nextConsumer.ConsumeLogs(ctx, ld)
}
