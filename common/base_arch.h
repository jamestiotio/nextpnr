/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2018  Clifford Wolf <clifford@symbioticeda.com>
 *  Copyright (C) 2018  Serge Bazanski <q3k@symbioticeda.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#ifndef BASE_ARCH_H
#define BASE_ARCH_H

#include <array>
#include <vector>

#include "arch_api.h"
#include "idstring.h"
#include "nextpnr_types.h"

NEXTPNR_NAMESPACE_BEGIN

namespace {
// For several functions; such as bel/wire/pip attributes; the trivial implementation is to return an empty vector
// But an arch might want to do something fancy with a custom range type that doesn't provide a constructor
// So some cursed C++ is needed to return an empty object if possible; or error out if not; is needed
template <typename Tc> typename std::enable_if<std::is_constructible<Tc>::value, Tc>::type empty_if_possible()
{
    return Tc();
}
template <typename Tc> typename std::enable_if<!std::is_constructible<Tc>::value, Tc>::type empty_if_possible()
{
    NPNR_ASSERT_FALSE("attempting to use default implementation of range-returning function with range type lacking "
                      "default constructor!");
}

// Provide a default implementation of bel bucket name if typedef'd to IdString
template <typename Tbbid>
typename std::enable_if<std::is_same<Tbbid, IdString>::value, IdString>::type bbid_to_name(Tbbid id)
{
    return id;
}
template <typename Tbbid>
typename std::enable_if<!std::is_same<Tbbid, IdString>::value, IdString>::type bbid_to_name(Tbbid id)
{
    NPNR_ASSERT_FALSE("getBelBucketName must be implemented when BelBucketId is a type other than IdString!");
}
template <typename Tbbid>
typename std::enable_if<std::is_same<Tbbid, IdString>::value, BelBucketId>::type bbid_from_name(IdString name)
{
    return name;
}
template <typename Tbbid>
typename std::enable_if<!std::is_same<Tbbid, IdString>::value, BelBucketId>::type bbid_from_name(IdString name)
{
    NPNR_ASSERT_FALSE("getBelBucketByName must be implemented when BelBucketId is a type other than IdString!");
}

// For the cell type and bel type ranges; we want to return our stored vectors only if the type matches
template <typename Tret, typename Tc>
typename std::enable_if<std::is_same<Tret, Tc>::value, Tret>::type return_if_match(Tret r)
{
    return r;
}

template <typename Tret, typename Tc>
typename std::enable_if<!std::is_same<Tret, Tc>::value, Tret>::type return_if_match(Tret r)
{
    NPNR_ASSERT_FALSE("default implementations of cell type and bel bucket range functions only available when the "
                      "respective range types are 'const std::vector&'");
}

} // namespace

// This contains the relevant range types for the default implementations of Arch functions
struct BaseArchRanges
{
    // Bels
    using CellBelPinRangeT = std::array<IdString, 1>;
    // Attributes
    using BelAttrsRangeT = std::vector<std::pair<IdString, std::string>>;
    using WireAttrsRangeT = std::vector<std::pair<IdString, std::string>>;
    using PipAttrsRangeT = std::vector<std::pair<IdString, std::string>>;
    // Groups
    using AllGroupsRangeT = std::vector<GroupId>;
    using GroupBelsRangeT = std::vector<BelId>;
    using GroupWiresRangeT = std::vector<WireId>;
    using GroupPipsRangeT = std::vector<PipId>;
    using GroupGroupsRangeT = std::vector<GroupId>;
    // Decals
    using DecalGfxRangeT = std::vector<GraphicElement>;
    // Placement validity
    using CellTypeRangeT = const std::vector<IdString> &;
    using BelBucketRangeT = const std::vector<BelBucketId> &;
    using BucketBelRangeT = const std::vector<BelId> &;
};

template <typename R> struct BaseArch : ArchAPI<R>
{
    // --------------------------------------------------------------
    // Default, trivial, implementations of Arch API functions for arches that don't need complex behaviours

    // Basic config
    virtual IdString archId() const override { return this->id(NPNR_STRINGIFY(ARCHNAME)); }
    virtual IdString archArgsToId(typename R::ArchArgsT args) const override { return IdString(); }
    virtual int getTilePipDimZ(int x, int y) const override { return 1; }
    virtual char getNameDelimiter() const override { return ' '; }

    // Bel methods
    virtual uint32_t getBelChecksum(BelId bel) const override { return uint32_t(std::hash<BelId>()(bel)); }
    virtual void bindBel(BelId bel, CellInfo *cell, PlaceStrength strength) override
    {
        NPNR_ASSERT(bel != BelId());
        auto &entry = base_bel2cell[bel];
        NPNR_ASSERT(entry == nullptr);
        cell->bel = bel;
        cell->belStrength = strength;
        entry = cell;
        this->refreshUiBel(bel);
    }
    virtual void unbindBel(BelId bel) override
    {
        NPNR_ASSERT(bel != BelId());
        auto &entry = base_bel2cell[bel];
        NPNR_ASSERT(entry != nullptr);
        entry->bel = BelId();
        entry->belStrength = STRENGTH_NONE;
        entry = nullptr;
        this->refreshUiBel(bel);
    }

    virtual bool getBelHidden(BelId bel) const override { return false; }

    virtual bool getBelGlobalBuf(BelId bel) const override { return false; }
    virtual bool checkBelAvail(BelId bel) const override { return getBoundBelCell(bel) == nullptr; };
    virtual CellInfo *getBoundBelCell(BelId bel) const override
    {
        auto fnd = base_bel2cell.find(bel);
        return fnd == base_bel2cell.end() ? nullptr : fnd->second;
    }
    virtual CellInfo *getConflictingBelCell(BelId bel) const override { return getBoundBelCell(bel); }
    virtual typename R::BelAttrsRangeT getBelAttrs(BelId bel) const override
    {
        return empty_if_possible<typename R::BelAttrsRangeT>();
    }

    virtual typename R::CellBelPinRangeT getBelPinsForCellPin(const CellInfo *cell_info, IdString pin) const override
    {
        return return_if_match<std::array<IdString, 1>, typename R::CellBelPinRangeT>({pin});
    }

    // Wire methods
    virtual IdString getWireType(WireId wire) const override { return IdString(); }
    virtual typename R::WireAttrsRangeT getWireAttrs(WireId) const override
    {
        return empty_if_possible<typename R::WireAttrsRangeT>();
    }
    virtual uint32_t getWireChecksum(WireId wire) const override { return uint32_t(std::hash<WireId>()(wire)); }

    virtual void bindWire(WireId wire, NetInfo *net, PlaceStrength strength) override
    {
        NPNR_ASSERT(wire != WireId());
        auto &w2n_entry = base_wire2net[wire];
        NPNR_ASSERT(w2n_entry == nullptr);
        net->wires[wire].pip = PipId();
        net->wires[wire].strength = strength;
        w2n_entry = net;
        this->refreshUiWire(wire);
    }
    virtual void unbindWire(WireId wire) override
    {
        NPNR_ASSERT(wire != WireId());
        auto &w2n_entry = base_wire2net[wire];
        NPNR_ASSERT(w2n_entry != nullptr);

        auto &net_wires = w2n_entry->wires;
        auto it = net_wires.find(wire);
        NPNR_ASSERT(it != net_wires.end());

        auto pip = it->second.pip;
        if (pip != PipId()) {
            base_pip2net[pip] = nullptr;
        }

        net_wires.erase(it);
        base_wire2net[wire] = nullptr;

        w2n_entry = nullptr;
        this->refreshUiWire(wire);
    }
    virtual bool checkWireAvail(WireId wire) const override { return getBoundWireNet(wire) == nullptr; }
    virtual NetInfo *getBoundWireNet(WireId wire) const override
    {
        auto fnd = base_wire2net.find(wire);
        return fnd == base_wire2net.end() ? nullptr : fnd->second;
    }
    virtual WireId getConflictingWireWire(WireId wire) const override { return wire; };
    virtual NetInfo *getConflictingWireNet(WireId wire) const override { return getBoundWireNet(wire); }

    // Pip methods
    virtual IdString getPipType(PipId pip) const override { return IdString(); }
    virtual typename R::PipAttrsRangeT getPipAttrs(PipId) const override
    {
        return empty_if_possible<typename R::PipAttrsRangeT>();
    }
    virtual uint32_t getPipChecksum(PipId pip) const override { return uint32_t(std::hash<PipId>()(pip)); }
    virtual void bindPip(PipId pip, NetInfo *net, PlaceStrength strength) override
    {
        NPNR_ASSERT(pip != PipId());
        auto &p2n_entry = base_pip2net[pip];
        NPNR_ASSERT(p2n_entry == nullptr);
        p2n_entry = net;

        WireId dst = this->getPipDstWire(pip);
        auto &w2n_entry = base_wire2net[dst];
        NPNR_ASSERT(w2n_entry == nullptr);
        w2n_entry = net;
        net->wires[dst].pip = pip;
        net->wires[dst].strength = strength;
    }
    virtual void unbindPip(PipId pip) override
    {
        NPNR_ASSERT(pip != PipId());
        auto &p2n_entry = base_pip2net[pip];
        NPNR_ASSERT(p2n_entry != nullptr);
        WireId dst = this->getPipDstWire(pip);

        auto &w2n_entry = base_wire2net[dst];
        NPNR_ASSERT(w2n_entry != nullptr);
        w2n_entry = nullptr;

        p2n_entry->wires.erase(dst);
        p2n_entry = nullptr;
    }
    virtual bool checkPipAvail(PipId pip) const override { return getBoundPipNet(pip) == nullptr; }
    virtual NetInfo *getBoundPipNet(PipId pip) const override
    {
        auto fnd = base_pip2net.find(pip);
        return fnd == base_pip2net.end() ? nullptr : fnd->second;
    }
    virtual WireId getConflictingPipWire(PipId pip) const override { return WireId(); }
    virtual NetInfo *getConflictingPipNet(PipId pip) const override { return getBoundPipNet(pip); }

    // Group methods
    virtual GroupId getGroupByName(IdStringList name) const override { return GroupId(); };
    virtual IdStringList getGroupName(GroupId group) const override { return IdStringList(); };
    virtual typename R::AllGroupsRangeT getGroups() const override
    {
        return empty_if_possible<typename R::AllGroupsRangeT>();
    }
    // Default implementation of these assumes no groups so never called
    virtual typename R::GroupBelsRangeT getGroupBels(GroupId group) const override
    {
        NPNR_ASSERT_FALSE("unreachable");
    };
    virtual typename R::GroupWiresRangeT getGroupWires(GroupId group) const override
    {
        NPNR_ASSERT_FALSE("unreachable");
    };
    virtual typename R::GroupPipsRangeT getGroupPips(GroupId group) const override
    {
        NPNR_ASSERT_FALSE("unreachable");
    };
    virtual typename R::GroupGroupsRangeT getGroupGroups(GroupId group) const override
    {
        NPNR_ASSERT_FALSE("unreachable");
    };

    // Delay methods
    virtual bool getBudgetOverride(const NetInfo *net_info, const PortRef &sink, delay_t &budget) const override
    {
        return false;
    }

    // Decal methods
    virtual typename R::DecalGfxRangeT getDecalGraphics(DecalId decal) const override
    {
        return empty_if_possible<typename R::DecalGfxRangeT>();
    };
    virtual DecalXY getBelDecal(BelId bel) const override { return DecalXY(); }
    virtual DecalXY getWireDecal(WireId wire) const override { return DecalXY(); }
    virtual DecalXY getPipDecal(PipId pip) const override { return DecalXY(); }
    virtual DecalXY getGroupDecal(GroupId group) const override { return DecalXY(); }

    // Cell timing methods
    virtual bool getCellDelay(const CellInfo *cell, IdString fromPort, IdString toPort, DelayQuad &delay) const override
    {
        return false;
    }
    virtual TimingPortClass getPortTimingClass(const CellInfo *cell, IdString port, int &clockInfoCount) const override
    {
        return TMG_IGNORE;
    }
    virtual TimingClockingInfo getPortClockingInfo(const CellInfo *cell, IdString port, int index) const override
    {
        NPNR_ASSERT_FALSE("unreachable");
    }

    // Placement validity checks
    virtual bool isValidBelForCellType(IdString cell_type, BelId bel) const override
    {
        return cell_type == this->getBelType(bel);
    }
    virtual IdString getBelBucketName(BelBucketId bucket) const override { return bbid_to_name<BelBucketId>(bucket); }
    virtual BelBucketId getBelBucketByName(IdString name) const override { return bbid_from_name<BelBucketId>(name); }
    virtual BelBucketId getBelBucketForBel(BelId bel) const override
    {
        return getBelBucketForCellType(this->getBelType(bel));
    };
    virtual BelBucketId getBelBucketForCellType(IdString cell_type) const override
    {
        return getBelBucketByName(cell_type);
    };
    virtual bool isBelLocationValid(BelId bel) const override { return true; }
    virtual typename R::CellTypeRangeT getCellTypes() const override
    {
        NPNR_ASSERT(cell_types_initialised);
        return return_if_match<const std::vector<IdString> &, typename R::CellTypeRangeT>(cell_types);
    }
    virtual typename R::BelBucketRangeT getBelBuckets() const override
    {
        NPNR_ASSERT(bel_buckets_initialised);
        return return_if_match<const std::vector<BelBucketId> &, typename R::BelBucketRangeT>(bel_buckets);
    }
    virtual typename R::BucketBelRangeT getBelsInBucket(BelBucketId bucket) const override
    {
        NPNR_ASSERT(bel_buckets_initialised);
        return return_if_match<const std::vector<BelId> &, typename R::BucketBelRangeT>(bucket_bels.at(bucket));
    }

    // Flow methods
    virtual void assignArchInfo() override{};

    // --------------------------------------------------------------
    // These structures are used to provide default implementations of bel/wire/pip binding. Arches might want to
    // replace them with their own, for example to use faster access structures than unordered_map. Arches might also
    // want to add extra checks around these functions
    std::unordered_map<BelId, CellInfo *> base_bel2cell;
    std::unordered_map<WireId, NetInfo *> base_wire2net;
    std::unordered_map<PipId, NetInfo *> base_pip2net;

    // For the default cell/bel bucket implementations
    std::vector<IdString> cell_types;
    std::vector<BelBucketId> bel_buckets;
    std::unordered_map<BelBucketId, std::vector<BelId>> bucket_bels;

    // Arches that want to use the default cell types and bel buckets *must* call these functions in their constructor
    bool cell_types_initialised = false;
    bool bel_buckets_initialised = false;
    void init_cell_types()
    {
        std::unordered_set<IdString> bel_types;
        for (auto bel : this->getBels())
            bel_types.insert(this->getBelType(bel));
        std::copy(bel_types.begin(), bel_types.end(), std::back_inserter(cell_types));
        std::sort(cell_types.begin(), cell_types.end());
        cell_types_initialised = true;
    }
    void init_bel_buckets()
    {
        for (auto cell_type : this->getCellTypes()) {
            auto bucket = this->getBelBucketForCellType(cell_type);
            bucket_bels[bucket]; // create empty bucket
        }
        for (auto bel : this->getBels()) {
            auto bucket = this->getBelBucketForBel(bel);
            bucket_bels[bucket].push_back(bel);
        }
        for (auto &b : bucket_bels)
            bel_buckets.push_back(b.first);
        std::sort(bel_buckets.begin(), bel_buckets.end());
        bel_buckets_initialised = true;
    }
};

NEXTPNR_NAMESPACE_END

#endif /* BASE_ARCH_H */
