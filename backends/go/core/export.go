package core

// Exported aliases of the request helpers so corefast (templ + go-json) runs exactly the same
// validation, session, URL and response logic as the html/template handlers.
var (
	ValidFE         = validFE
	IsHTMX          = isHTMX
	ToInt           = toInt
	ExistingSession = existingSession
	SessionID       = sessionID
	ValidTitle      = validTitle
	SafeReferer     = safeReferer
	ValidColumn     = validColumn
	ParseQuery      = parseQuery
	ParseRange      = parseRange
	Href            = href
	ColumnLabels    = columnLabels

	NotFoundHTML  = notFoundHTML
	NotFoundJSON  = notFoundJSON
	BadJSON       = badJSON
	Unprocessable = unprocessable
	HTMLStatus    = htmlStatus
	Redirect      = redirect
	Healthz       = healthz
)

// View renders a named html/template; corefast's tests use it as the reference output.
var View = view

// View-model builders, exported for corefast's reference-output tests.
var (
	ProductVMMap = productVM
	CartVMMap    = cartVM
	BoardVMMap   = boardVM
)
