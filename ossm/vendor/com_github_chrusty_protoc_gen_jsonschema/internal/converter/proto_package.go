package converter

import (
	"strings"

	descriptor "google.golang.org/protobuf/types/descriptorpb"
)

// ProtoPackage describes a package of Protobuf, which is an container of message types.
type ProtoPackage struct {
	name     string
	parent   *ProtoPackage
	children map[string]*ProtoPackage
	types    map[string]*descriptor.DescriptorProto
	enums    map[string]*descriptor.EnumDescriptorProto
}

func newProtoPackage(parent *ProtoPackage, name string) *ProtoPackage {
	pkgName := name
	if parent != nil {
		pkgName = parent.name + "." + name
	}

	return &ProtoPackage{
		name:     pkgName,
		parent:   parent,
		children: make(map[string]*ProtoPackage),
		types:    make(map[string]*descriptor.DescriptorProto),
		enums:    make(map[string]*descriptor.EnumDescriptorProto),
	}
}

func (c *Converter) lookupType(pkg *ProtoPackage, name string) (*descriptor.DescriptorProto, string, bool) {
	if strings.HasPrefix(name, ".") {
		return c.relativelyLookupType(globalPkg, name[1:])
	}

	for ; pkg != nil; pkg = pkg.parent {
		if desc, pkgName, ok := c.relativelyLookupType(pkg, name); ok {
			return desc, pkgName, ok
		}
	}
	return nil, "", false
}

func (c *Converter) lookupEnum(pkg *ProtoPackage, name string) (*descriptor.EnumDescriptorProto, string, bool) {
	if strings.HasPrefix(name, ".") {
		return c.relativelyLookupEnum(globalPkg, name[1:])
	}

	for ; pkg != nil; pkg = pkg.parent {
		if desc, pkgName, ok := c.relativelyLookupEnum(pkg, name); ok {
			return desc, pkgName, ok
		}
	}
	return nil, "", false
}

func (c *Converter) relativelyLookupType(pkg *ProtoPackage, name string) (*descriptor.DescriptorProto, string, bool) {
	components := strings.SplitN(name, ".", 2)
	switch len(components) {
	case 0:
		c.logger.Debug("empty message name")
		return nil, "", false
	case 1:
		found, ok := pkg.types[components[0]]
		return found, pkg.name, ok
	case 2:
		c.logger.Tracef("Looking for %s in %s at %s (%v)", components[1], components[0], pkg.name, pkg)
		if child, ok := pkg.children[components[0]]; ok {
			found, pkgName, ok := c.relativelyLookupType(child, components[1])
			return found, pkgName, ok
		}
		if msg, ok := pkg.types[components[0]]; ok {
			found, ok := c.relativelyLookupNestedType(msg, components[1])
			return found, pkg.name, ok
		}
		c.logger.WithField("component", components[0]).WithField("package_name", pkg.name).Debug("No such package nor message in package")
		return nil, "", false
	default:
		c.logger.Error("Failed to lookup type")
		return nil, "", false
	}
}

func (c *Converter) relativelyLookupNestedType(desc *descriptor.DescriptorProto, name string) (*descriptor.DescriptorProto, bool) {
	components := strings.Split(name, ".")
componentLoop:
	for _, component := range components {
		for _, nested := range desc.GetNestedType() {
			if nested.GetName() == component {
				desc = nested
				continue componentLoop
			}
		}
		c.logger.WithField("component", component).WithField("description", desc.GetName()).Info("no such nested message")
		return nil, false
	}
	return desc, true
}

func (c *Converter) relativelyLookupEnum(pkg *ProtoPackage, name string) (*descriptor.EnumDescriptorProto, string, bool) {
	components := strings.SplitN(name, ".", 2)
	switch len(components) {
	case 0:
		c.logger.Debug("empty enum name")
		return nil, "", false
	case 1:
		found, ok := pkg.enums[components[0]]
		return found, pkg.name, ok
	case 2:
		c.logger.Tracef("Looking for %s in %s at %s (%v)", components[1], components[0], pkg.name, pkg)
		if child, ok := pkg.children[components[0]]; ok {
			found, pkgName, ok := c.relativelyLookupEnum(child, components[1])
			return found, pkgName, ok
		}
		if msg, ok := pkg.types[components[0]]; ok {
			found, ok := c.relativelyLookupNestedEnum(msg, components[1])
			return found, pkg.name, ok
		}
		c.logger.WithField("component", components[0]).WithField("package_name", pkg.name).Debug("No such package nor message in package")
		return nil, "", false
	default:
		c.logger.Error("Failed to lookup type")
		return nil, "", false
	}
}

func (c *Converter) relativelyLookupNestedEnum(desc *descriptor.DescriptorProto, name string) (*descriptor.EnumDescriptorProto, bool) {
	components := strings.Split(name, ".")

	parent := desc

	if len(components) > 1 {
		// The enum is nested inside a potentially nested message definition.
		msgComponents := strings.Join(components[0:len(components)-1], ".")
		var found bool
		parent, found = c.relativelyLookupNestedType(parent, msgComponents)
		if !found {
			return nil, false
		}
	}

	// The enum is nested inside of a nested message. We need to dive down the
	// tree to find the message the enum is nested in. Then we need to obtain the
	// enum.
	enumName := components[len(components)-1]
	for _, enum := range parent.GetEnumType() {
		if enum.GetName() == enumName {
			return enum, true
		}
	}

	return nil, false
}

func (c *Converter) relativelyLookupPackage(pkg *ProtoPackage, name string) (*ProtoPackage, bool) {
	components := strings.Split(name, ".")
	for _, c := range components {
		var ok bool
		pkg, ok = pkg.children[c]
		if !ok {
			return nil, false
		}
	}
	return pkg, true
}
