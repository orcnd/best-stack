package main

import (
	"net/http/httptest"
	"testing"
)

// The core panics on DB errors (here: DB never initialised). fasthttp has no per-connection recovery,
// so without recover middleware one panic would end the whole process.
func TestPanicBecomes500(t *testing.T) {
	resp, err := newApp().Test(httptest.NewRequest("GET", "/api/dashboard", nil))
	if err != nil {
		t.Fatalf("request failed (panic escaped): %v", err)
	}
	if resp.StatusCode != 500 {
		t.Fatalf("status %d, want 500", resp.StatusCode)
	}
}
