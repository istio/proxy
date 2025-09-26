package pgs

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"google.golang.org/protobuf/proto"
	descriptor "google.golang.org/protobuf/types/descriptorpb"
)

func TestSourceCodeInfo(t *testing.T) {
	t.Parallel()

	desc := &descriptor.SourceCodeInfo_Location{
		LeadingComments:         proto.String("leading"),
		TrailingComments:        proto.String("trailing"),
		LeadingDetachedComments: []string{"detached"},
	}

	info := sci{desc}

	assert.Equal(t, desc, info.Location())
	assert.Equal(t, "leading", info.LeadingComments())
	assert.Equal(t, "trailing", info.TrailingComments())
	assert.Equal(t, []string{"detached"}, info.LeadingDetachedComments())
}
