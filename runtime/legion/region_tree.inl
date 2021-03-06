/* Copyright 2019 Stanford University, NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

namespace Legion {
  namespace Internal {

    LEGION_EXTERN_LOGGER_DECLARATIONS

#ifdef DEFINE_NT_TEMPLATES
    /////////////////////////////////////////////////////////////
    // Index Space Expression 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceExpression::issue_fill_internal(
                                 RegionTreeForest *forest,
                                 const Realm::IndexSpace<DIM,T> &space,
                                 const PhysicalTraceInfo &trace_info,
                                 const std::vector<CopySrcDstField> &dst_fields,
                                 const void *fill_value, size_t fill_size,
#ifdef LEGION_SPY
                                 UniqueID fill_uid,
                                 FieldSpace handle,
                                 RegionTreeID tree_id,
#endif
                                 ApEvent precondition, PredEvent pred_guard,
                                 const FieldMaskSet<FillView> *tracing_srcs,
                                 const FieldMaskSet<InstanceView> *tracing_dsts)
    //--------------------------------------------------------------------------
    {
      DETAILED_PROFILER(forest->runtime, REALM_ISSUE_FILL_CALL);
#ifdef DEBUG_LEGION
      // We should only have empty spaces for fills that are indirections
      if (space.empty())
      {
        bool is_indirect = false;
        for (unsigned idx = 0; idx < dst_fields.size(); idx++)
        {
          if (dst_fields[idx].indirect_index < 0)
            continue;
          is_indirect = true;
          break;
        }
        assert(is_indirect);
      }
#endif
      // Now that we know we're going to do this fill add any profiling requests
      Realm::ProfilingRequestSet requests;
      if (trace_info.op != NULL)
        trace_info.op->add_copy_profiling_request(requests);
      if (forest->runtime->profiler != NULL)
        forest->runtime->profiler->add_fill_request(requests, trace_info.op);
#ifdef LEGION_SPY
      // Have to convert back to Realm data structures because C++ is dumb
      std::vector<Realm::CopySrcDstField> realm_dst_fields(dst_fields.size());
      for (unsigned idx = 0; idx < dst_fields.size(); idx++)
        realm_dst_fields[idx] = dst_fields[idx];
#endif
      ApEvent result;
      if (pred_guard.exists())
      {
        ApEvent pred_pre = 
          Runtime::merge_events(&trace_info, precondition, ApEvent(pred_guard));
        if (trace_info.recording)
          trace_info.record_merge_events(pred_pre, precondition,
                                          ApEvent(pred_guard));
#ifdef LEGION_SPY
        result = Runtime::ignorefaults(space.fill(realm_dst_fields, requests, 
                                              fill_value, fill_size, pred_pre));
#else
        result = Runtime::ignorefaults(space.fill(dst_fields, requests, 
                                              fill_value, fill_size, pred_pre));
#endif                               
      }
      else
      {
#ifdef LEGION_SPY
        result = ApEvent(space.fill(realm_dst_fields, requests, 
                                    fill_value, fill_size, precondition));
#else
        result = ApEvent(space.fill(dst_fields, requests, 
                                    fill_value, fill_size, precondition));
#endif
      }
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists())
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      // We can skip this if fill_uid is 0
      if (fill_uid != 0)
      {
        assert(trace_info.op != NULL);
        LegionSpy::log_fill_events(trace_info.op->get_unique_op_id(), 
            expr_id, handle, tree_id, precondition, result, fill_uid);
        for (unsigned idx = 0; idx < dst_fields.size(); idx++)
          LegionSpy::log_fill_field(result, dst_fields[idx].field_id,
                                    dst_fields[idx].inst_event);
      }
#endif
      if (trace_info.recording)
      {
        trace_info.record_issue_fill(result, this, dst_fields,
                                     fill_value, fill_size,
#ifdef LEGION_SPY
                                     handle, tree_id,
#endif
                                     precondition, tracing_srcs, tracing_dsts);
      }
      return result;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceExpression::issue_copy_internal(
                                 RegionTreeForest *forest,
                                 const Realm::IndexSpace<DIM,T> &space,
                                 const PhysicalTraceInfo &trace_info,
                                 const std::vector<CopySrcDstField> &dst_fields,
                                 const std::vector<CopySrcDstField> &src_fields,
#ifdef LEGION_SPY
                                 FieldSpace handle,
                                 RegionTreeID src_tree_id,
                                 RegionTreeID dst_tree_id,
#endif
                                 ApEvent precondition, PredEvent pred_guard,
                                 ReductionOpID redop, bool reduction_fold,
                                 const FieldMaskSet<InstanceView> *tracing_srcs,
                                 const FieldMaskSet<InstanceView> *tracing_dsts)
    //--------------------------------------------------------------------------
    {
      DETAILED_PROFILER(forest->runtime, REALM_ISSUE_COPY_CALL);
#ifdef DEBUG_LEGION
      // We should only have empty spaces for copies that are indirections
      if (space.empty())
      {
        // Only check for non-empty spaces on copies without indirections
        bool is_indirect = false;
        for (unsigned idx = 0; idx < src_fields.size(); idx++)
        {
          if (src_fields[idx].indirect_index < 0)
            continue;
          is_indirect = true;
          break;
        }
        if (!is_indirect)
        {
          for (unsigned idx = 0; idx < dst_fields.size(); idx++)
          {
            if (dst_fields[idx].indirect_index < 0)
              continue;
            is_indirect = true;
            break;
          }
          assert(is_indirect);
        }
      }
#endif
      // Now that we know we're going to do this copy add any profling requests
      Realm::ProfilingRequestSet requests;
      if (trace_info.op != NULL)
        trace_info.op->add_copy_profiling_request(requests);
      if (forest->runtime->profiler != NULL)
        forest->runtime->profiler->add_copy_request(requests, trace_info.op);
#ifdef LEGION_SPY
      // Have to convert back to Realm structures because C++ is dumb  
      std::vector<Realm::CopySrcDstField> realm_src_fields(src_fields.size());
      for (unsigned idx = 0; idx < src_fields.size(); idx++)
        realm_src_fields[idx] = src_fields[idx];
      std::vector<Realm::CopySrcDstField> realm_dst_fields(dst_fields.size());
      for (unsigned idx = 0; idx < dst_fields.size(); idx++)
        realm_dst_fields[idx] = dst_fields[idx];
#endif 
      ApEvent result;
      if (pred_guard.exists())
      {
        ApEvent pred_pre = 
          Runtime::merge_events(&trace_info, precondition, ApEvent(pred_guard));
        if (trace_info.recording)
          trace_info.record_merge_events(pred_pre, precondition,
                                          ApEvent(pred_guard));
#ifdef LEGION_SPY
        result = Runtime::ignorefaults(space.copy(realm_src_fields, 
              realm_dst_fields, requests, pred_pre, redop, reduction_fold));
#else
        result = Runtime::ignorefaults(space.copy(src_fields, dst_fields, 
                                requests, pred_pre, redop, reduction_fold));
#endif
      }
      else
      {
#ifdef LEGION_SPY
        result = ApEvent(space.copy(realm_src_fields, realm_dst_fields, 
                          requests, precondition, redop, reduction_fold));
#else
        result = ApEvent(space.copy(src_fields, dst_fields, requests, 
                          precondition, redop, reduction_fold));
#endif
      }
      if (trace_info.recording)
      {
        trace_info.record_issue_copy(result, this, src_fields, dst_fields,
#ifdef LEGION_SPY
                                     handle, src_tree_id, dst_tree_id,
#endif
                                     precondition, redop, reduction_fold,
                                     tracing_srcs, tracing_dsts);
      }
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists())
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      assert(trace_info.op != NULL);
      LegionSpy::log_copy_events(trace_info.op->get_unique_op_id(), 
          expr_id, handle, src_tree_id, dst_tree_id, precondition, result);
      for (unsigned idx = 0; idx < src_fields.size(); idx++)
        LegionSpy::log_copy_field(result, src_fields[idx].field_id,
                                  src_fields[idx].inst_event,
                                  dst_fields[idx].field_id,
                                  dst_fields[idx].inst_event, redop);
#endif
      return result;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceExpression::construct_indirections_internal(
                                     const std::vector<unsigned> &field_indexes,
                                     const FieldID indirect_field,
                                     const TypeTag indirect_type,
                                     const bool is_range,
                                     const PhysicalInstance indirect_instance,
                                     const LegionVector<
                                            IndirectRecord>::aligned &records,
                                     std::vector<void*> &indirects,
                                     std::vector<unsigned> &indirect_indexes)
    //--------------------------------------------------------------------------
    {
      typedef std::vector<typename Realm::CopyIndirection<DIM,T>::Base*>
        IndirectionVector;
      IndirectionVector &indirections = 
        *reinterpret_cast<IndirectionVector*>(&indirects);
      // Sort instances into field sets and
      FieldMaskSet<IndirectRecord> record_sets;
      for (unsigned idx = 0; idx < records.size(); idx++)
        record_sets.insert(const_cast<IndirectRecord*>(&records[idx]), 
                           records[idx].fields);
#ifdef DEBUG_LEGION
      // Little sanity check here that all fields are represented
      assert(unsigned(record_sets.get_valid_mask().pop_count()) == 
              field_indexes.size());
#endif
      // construct indirections for each field set
      LegionList<FieldSet<IndirectRecord*> >::aligned field_sets;
      record_sets.compute_field_sets(FieldMask(), field_sets);
      // Note that we might be appending to some existing indirections
      const unsigned offset = indirections.size();
      indirections.resize(offset+field_sets.size());
      unsigned index = 0;
      for (LegionList<FieldSet<IndirectRecord*> >::aligned::const_iterator it =
            field_sets.begin(); it != field_sets.end(); it++, index++)
      {
        UnstructuredIndirectionHelper<DIM,T> helper(indirect_field, is_range,
                                              indirect_instance, it->elements);
        NT_TemplateHelper::demux<UnstructuredIndirectionHelper<DIM,T> >(
            indirect_type, &helper);
        indirections[offset+index] = helper.result;
      }
      // For each field find it's indirection and record it
#ifdef DEBUG_LEGION
      assert(indirect_indexes.empty());
#endif
      indirect_indexes.resize(field_indexes.size());  
      for (unsigned idx = 0; idx < field_indexes.size(); idx++)
      {
        const unsigned fidx = field_indexes[idx];
        // Search through the set of indirections and find the one that is
        // set for this field
        index = 0;
        for (LegionList<FieldSet<IndirectRecord*> >::aligned::const_iterator
              it = field_sets.begin(); it != field_sets.end(); it++, index++)
        {
          if (!it->set_mask.is_set(fidx))
            continue;
          indirect_indexes[idx] = offset+index;
          break;
        }
#ifdef DEBUG_LEGION
        // Should have found it in the set
        assert(index < field_sets.size());
#endif
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceExpression::destroy_indirections_internal(
                                                  std::vector<void*> &indirects)
    //--------------------------------------------------------------------------
    {
      typedef std::vector<typename Realm::CopyIndirection<DIM,T>::Base*>
        IndirectionVector;
      IndirectionVector &indirections = 
        *reinterpret_cast<IndirectionVector*>(&indirects);
      for (unsigned idx = 0; idx < indirections.size(); idx++)
        delete indirections[idx];
      indirects.clear();
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceExpression::issue_indirect_internal(
                                 RegionTreeForest *forest,
                                 const Realm::IndexSpace<DIM,T> &space,
                                 const PhysicalTraceInfo &trace_info,
                                 const std::vector<CopySrcDstField> &dst_fields,
                                 const std::vector<CopySrcDstField> &src_fields,
                                 const std::vector<void*> &indirects,
                                 ApEvent precondition, PredEvent pred_guard)
    //--------------------------------------------------------------------------
    {
      // Now that we know we're going to do this copy add any profling requests
      Realm::ProfilingRequestSet requests;
      if (trace_info.op != NULL)
        trace_info.op->add_copy_profiling_request(requests);
      if (forest->runtime->profiler != NULL)
        forest->runtime->profiler->add_copy_request(requests, trace_info.op);
#ifdef LEGION_SPY
      // Have to convert back to Realm structures because C++ is dumb  
      std::vector<Realm::CopySrcDstField> realm_src_fields(src_fields.size());
      for (unsigned idx = 0; idx < src_fields.size(); idx++)
        realm_src_fields[idx] = src_fields[idx];
      std::vector<Realm::CopySrcDstField> realm_dst_fields(dst_fields.size());
      for (unsigned idx = 0; idx < dst_fields.size(); idx++)
        realm_dst_fields[idx] = dst_fields[idx];
#endif 
      typedef std::vector<const typename Realm::CopyIndirection<DIM,T>::Base*>
        IndirectionVector;
      const IndirectionVector &indirections = 
        *reinterpret_cast<const IndirectionVector*>(&indirects);
      ApEvent result;
      if (pred_guard.exists())
      {
        ApEvent pred_pre = 
          Runtime::merge_events(&trace_info, precondition, ApEvent(pred_guard));
        if (trace_info.recording)
          trace_info.record_merge_events(pred_pre, precondition,
                                          ApEvent(pred_guard));
#ifdef LEGION_SPY
        result = Runtime::ignorefaults(space.copy(realm_src_fields, 
                          realm_dst_fields, indirections, requests, pred_pre));
#else
        result = Runtime::ignorefaults(space.copy(src_fields, dst_fields, 
                                            indirections, requests, pred_pre));
#endif
      }
      else
      {
#ifdef LEGION_SPY
        result = ApEvent(space.copy(realm_src_fields, realm_dst_fields, 
                                    indirections, requests, precondition));
#else
        result = ApEvent(space.copy(src_fields, dst_fields, indirections,
                                    requests, precondition));
#endif
      }
      if (trace_info.recording)
        trace_info.record_issue_indirect(result, this, src_fields, dst_fields,
                                         indirects, precondition);
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists())
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      assert(trace_info.op != NULL);
#if 0
      LegionSpy::log_copy_events(trace_info.op->get_unique_op_id(), 
          expr_id, handle, src_tree_id, dst_tree_id, precondition, result);
      for (unsigned idx = 0; idx < src_fields.size(); idx++)
        LegionSpy::log_copy_field(result, src_fields[idx].field_id,
                                  src_fields[idx].inst_event,
                                  dst_fields[idx].field_id,
                                  dst_fields[idx].inst_event, redop);
#else
      // TODO: Legion Spy for indirect copies
      assert(false);
#endif
#endif
      return result;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    Realm::InstanceLayoutGeneric* IndexSpaceExpression::create_layout_internal(
                                    const Realm::IndexSpace<DIM,T> &space,
                                    const Realm::InstanceLayoutConstraints &ilc,
                                    const OrderingConstraint &constraint) const
    //--------------------------------------------------------------------------
    {
      int dim_order[DIM];
      // Construct the dimension ordering
      unsigned next_dim = 0;
      for (std::vector<DimensionKind>::const_iterator it = 
            constraint.ordering.begin(); it != constraint.ordering.end(); it++)
      {
        // Skip the field dimension we already handled it
        if ((*it) == DIM_F)
          continue;
        if ((*it) > DIM_F)
          assert(false); // TODO: handle split dimensions
        if ((*it) >= DIM) // Skip dimensions bigger than ours
          continue;
        dim_order[next_dim++] = *it;
      }
#ifdef DEBUG_LEGION
      assert(next_dim == DIM); // should have filled them all in
#endif
      return Realm::InstanceLayoutGeneric::choose_instance_layout(space,
                                                            ilc, dim_order);
    }

    /////////////////////////////////////////////////////////////
    // Index Space Operations 
    /////////////////////////////////////////////////////////////
    
    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceOperationT<DIM,T>::IndexSpaceOperationT(OperationKind kind,
                                                      RegionTreeForest *ctx)
      : IndexSpaceOperation(NT_TemplateHelper::encode_tag<DIM,T>(),
                            kind, ctx), is_index_space_tight(false)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceOperationT<DIM,T>::IndexSpaceOperationT(OperationKind kind,
                                  RegionTreeForest *ctx, Deserializer &derez)
      : IndexSpaceOperation(NT_TemplateHelper::encode_tag<DIM,T>(),
                            kind, ctx, derez), is_index_space_tight(false)
    //--------------------------------------------------------------------------
    {
      // We can unpack the index space here directly
      derez.deserialize(this->realm_index_space);
      this->tight_index_space = this->realm_index_space;
      derez.deserialize(this->realm_index_space_ready);
      // We'll do the make_valid request in activate_remote if needed
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceOperationT<DIM,T>::~IndexSpaceOperationT(void)
    //--------------------------------------------------------------------------
    {
      if (this->origin_space == this->context->runtime->address_space)
      {
        this->realm_index_space.destroy(realm_index_space_ready);
        this->tight_index_space.destroy(tight_index_space_ready);
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceOperationT<DIM,T>::get_expr_index_space(void *result,
                                            TypeTag tag, bool need_tight_result)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(tag == type_tag);
#endif
      Realm::IndexSpace<DIM,T> *space = 
        reinterpret_cast<Realm::IndexSpace<DIM,T>*>(result);
      return get_realm_index_space(*space, need_tight_result);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    Domain IndexSpaceOperationT<DIM,T>::get_domain(ApEvent &ready, bool tight)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> result;
      ready = get_realm_index_space(result, tight);
      return DomainT<DIM,T>(result);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceOperationT<DIM,T>::get_realm_index_space(
                        Realm::IndexSpace<DIM,T> &space, bool need_tight_result)
    //--------------------------------------------------------------------------
    {
      if (!is_index_space_tight)
      {
        if (need_tight_result)
        {
          // Wait for the index space to be tight
          if (tight_index_space_ready.exists() && 
              !tight_index_space_ready.has_triggered())
            tight_index_space_ready.wait();
          space = tight_index_space;
          return ApEvent::NO_AP_EVENT;
        }
        else
        {
          space = realm_index_space;
          return realm_index_space_ready;
        }
      }
      else
      {
        // Already tight so we can just return that
        space = tight_index_space;
        return ApEvent::NO_AP_EVENT;
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceOperationT<DIM,T>::tighten_index_space(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(realm_index_space.is_valid());
#endif
      tight_index_space = realm_index_space.tighten();
#ifdef DEBUG_LEGION
      assert(tight_index_space.is_valid());
#endif
      // Small memory fence to propagate writes before setting the flag
      __sync_synchronize();
      is_index_space_tight = true;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexSpaceOperationT<DIM,T>::check_empty(void)
    //--------------------------------------------------------------------------
    {
      return (get_volume() == 0);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    size_t IndexSpaceOperationT<DIM,T>::get_volume(void)
    //--------------------------------------------------------------------------
    {
      if (has_volume)
        return volume;
      Realm::IndexSpace<DIM,T> temp;
      ApEvent ready = get_realm_index_space(temp, true/*tight*/);
      if (ready.exists() && !ready.has_triggered())
        ready.wait();
      volume = temp.volume();
      __sync_synchronize();
      has_volume = true;
      return volume;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceOperationT<DIM,T>::activate_remote(void)
    //--------------------------------------------------------------------------
    {
      // Request that we make the valid index space valid
      this->tight_index_space_ready = 
        RtEvent(this->realm_index_space.make_valid());
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceOperationT<DIM,T>::pack_expression(Serializer &rez,
                                                      AddressSpaceID target)
    //--------------------------------------------------------------------------
    {
      if (target == context->runtime->address_space)
      {
        rez.serialize<bool>(true/*local*/);
        rez.serialize(this);
      }
      else if (target == origin_space)
      {
        rez.serialize<bool>(true/*local*/);
        rez.serialize(origin_expr);
      }
      else
      {
        rez.serialize<bool>(false/*local*/);
        rez.serialize<bool>(false/*index space*/);
        rez.serialize(expr_id);
        rez.serialize(origin_expr);
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceNode* IndexSpaceOperationT<DIM,T>::find_or_create_node(
                                                               TaskContext *ctx)
    //--------------------------------------------------------------------------
    {
      if (node != NULL)
        return node;
      Runtime *runtime = context->runtime;
      {
        AutoLock i_lock(inter_lock);
        // Retest after we get the lock
        if (node != NULL)
          return node;
        // Make a handle and DID to use for this index space
        IndexSpace handle = runtime->help_create_index_space_handle(type_tag);
        DistributedID did = runtime->get_available_distributed_id();
#ifdef DEBUG_LEGION
        if (ctx != NULL)
          log_index.debug("Creating index space %x in task%s (ID %lld)", 
                handle.get_id(), ctx->get_task_name(), ctx->get_unique_id());
#endif
        
        if (is_index_space_tight)
          node = context->create_node(handle, &tight_index_space,
                                      NULL/*parent*/, 0/*color*/, did,
                                      realm_index_space_ready, expr_id);
        else
          node = context->create_node(handle, &realm_index_space,
                                      NULL/*parent*/, 0/*color*/, did,
                                      realm_index_space_ready, expr_id);
      }
      if (ctx != NULL)
        ctx->register_index_space_creation(node->handle);
      if (runtime->legion_spy_enabled)
        LegionSpy::log_top_index_space(node->handle.get_id());
      return node;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceOperationT<DIM,T>::issue_fill(
                                 const PhysicalTraceInfo &trace_info,
                                 const std::vector<CopySrcDstField> &dst_fields,
                                 const void *fill_value, size_t fill_size,
#ifdef LEGION_SPY
                                 UniqueID fill_uid,
                                 FieldSpace handle,
                                 RegionTreeID tree_id,
#endif
                                 ApEvent precondition, PredEvent pred_guard,
                                 const FieldMaskSet<FillView> *tracing_srcs,
                                 const FieldMaskSet<InstanceView> *tracing_dsts)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> local_space;
      ApEvent space_ready = get_realm_index_space(local_space, true/*tight*/);
      if (space_ready.exists() && precondition.exists())
        return issue_fill_internal(context, local_space, trace_info, 
            dst_fields, fill_value, fill_size, 
#ifdef LEGION_SPY
            fill_uid, handle, tree_id,
#endif
            Runtime::merge_events(&trace_info, space_ready, precondition), 
            pred_guard, tracing_srcs, tracing_dsts);
      else if (space_ready.exists())
        return issue_fill_internal(context, local_space, trace_info, 
                                   dst_fields, fill_value, fill_size,
#ifdef LEGION_SPY
                                   fill_uid, handle, tree_id,
#endif
                                   space_ready, pred_guard,
                                   tracing_srcs, tracing_dsts);
      else
        return issue_fill_internal(context, local_space, trace_info, 
                                   dst_fields, fill_value, fill_size,
#ifdef LEGION_SPY
                                   fill_uid, handle, tree_id,
#endif
                                   precondition, pred_guard,
                                   tracing_srcs, tracing_dsts);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceOperationT<DIM,T>::issue_copy(
                                 const PhysicalTraceInfo &trace_info,
                                 const std::vector<CopySrcDstField> &dst_fields,
                                 const std::vector<CopySrcDstField> &src_fields,
#ifdef LEGION_SPY
                                 FieldSpace handle, 
                                 RegionTreeID src_tree_id,
                                 RegionTreeID dst_tree_id,
#endif
                                 ApEvent precondition, PredEvent pred_guard,
                                 ReductionOpID redop, bool reduction_fold,
                                 const FieldMaskSet<InstanceView> *tracing_srcs,
                                 const FieldMaskSet<InstanceView> *tracing_dsts)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> local_space;
      ApEvent space_ready = get_realm_index_space(local_space, true/*tight*/);
      if (space_ready.exists() && precondition.exists())
        return issue_copy_internal(context, local_space, trace_info, 
            dst_fields, src_fields,
#ifdef LEGION_SPY
            handle, src_tree_id, dst_tree_id,
#endif
            Runtime::merge_events(&trace_info, precondition, space_ready),
            pred_guard, redop, reduction_fold, tracing_srcs, tracing_dsts);
      else if (space_ready.exists())
        return issue_copy_internal(context, local_space, trace_info, 
                dst_fields, src_fields, 
#ifdef LEGION_SPY
                handle, src_tree_id, dst_tree_id,
#endif
                space_ready, pred_guard, redop, reduction_fold,
                tracing_srcs, tracing_dsts);
      else
        return issue_copy_internal(context, local_space, trace_info, 
                dst_fields, src_fields, 
#ifdef LEGION_SPY
                handle, src_tree_id, dst_tree_id,
#endif
                precondition, pred_guard, redop, reduction_fold,
                tracing_srcs, tracing_dsts);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceOperationT<DIM,T>::construct_indirections(
                                     const std::vector<unsigned> &field_indexes,
                                     const FieldID indirect_field,
                                     const TypeTag indirect_type,
                                     const bool is_range,
                                     const PhysicalInstance indirect_instance,
                                     const LegionVector<
                                            IndirectRecord>::aligned &records,
                                     std::vector<void*> &indirections,
                                     std::vector<unsigned> &indirect_indexes)
    //--------------------------------------------------------------------------
    {
      construct_indirections_internal<DIM,T>(field_indexes, indirect_field,
                                 indirect_type, is_range, indirect_instance, 
                                 records, indirections, indirect_indexes);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceOperationT<DIM,T>::destroy_indirections(
                                               std::vector<void*> &indirections)
    //--------------------------------------------------------------------------
    {
      destroy_indirections_internal<DIM,T>(indirections);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceOperationT<DIM,T>::issue_indirect(
                                 const PhysicalTraceInfo &trace_info,
                                 const std::vector<CopySrcDstField> &dst_fields,
                                 const std::vector<CopySrcDstField> &src_fields,
                                 const std::vector<void*> &indirects,
                                 ApEvent precondition, PredEvent pred_guard)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> local_space;
      ApEvent space_ready = get_realm_index_space(local_space, true/*tight*/);
      if (space_ready.exists() && precondition.exists())
        return issue_indirect_internal(context, local_space, trace_info, 
            dst_fields, src_fields, indirects,
            Runtime::merge_events(&trace_info, precondition, space_ready),
            pred_guard);
      else if (space_ready.exists())
        return issue_indirect_internal(context, local_space, trace_info, 
                                       dst_fields, src_fields, indirects, 
                                       space_ready, pred_guard);
      else
        return issue_indirect_internal(context, local_space, trace_info, 
                                       dst_fields, src_fields, indirects,
                                       precondition, pred_guard);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    Realm::InstanceLayoutGeneric* IndexSpaceOperationT<DIM,T>::create_layout(
                                    const Realm::InstanceLayoutConstraints &ilc,
                                    const OrderingConstraint &constraint)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> local_space;
      ApEvent space_ready = get_realm_index_space(local_space, true/*tight*/);
      if (space_ready.exists())
        space_ready.wait();
      return create_layout_internal(local_space, ilc, constraint);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceUnion<DIM,T>::IndexSpaceUnion(
                            const std::vector<IndexSpaceExpression*> &to_union,
                            RegionTreeForest *ctx)
      : IndexSpaceOperationT<DIM,T>(IndexSpaceOperation::UNION_OP_KIND, ctx),
        sub_expressions(to_union)
    //--------------------------------------------------------------------------
    {
      std::set<ApEvent> preconditions;
      std::vector<Realm::IndexSpace<DIM,T> > spaces(sub_expressions.size());
      for (unsigned idx = 0; idx < sub_expressions.size(); idx++)
      {
        IndexSpaceExpression *sub = sub_expressions[idx];
        // Add the parent and the reference
        sub->add_parent_operation(this);
        sub->add_expression_reference();
        // Then get the realm index space expression
        ApEvent precondition = sub->get_expr_index_space(
            &spaces[idx], this->type_tag, false/*need tight result*/);
        if (precondition.exists())
          preconditions.insert(precondition);
      }
      // Kick this off to Realm
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      Realm::ProfilingRequestSet requests;
      if (ctx->runtime->profiler != NULL)
        ctx->runtime->profiler->add_partition_request(requests,
                      implicit_provenance, DEP_PART_UNION_REDUCTION);
      this->realm_index_space_ready = ApEvent(
          Realm::IndexSpace<DIM,T>::compute_union(
              spaces, this->realm_index_space, requests, precondition));
      // Then launch the tighten call for it too since we know we're
      // going to want this eventually
      const RtEvent valid_event(this->realm_index_space.make_valid());
      // See if both the events needed for the tighten call are done
      if (!this->realm_index_space_ready.has_triggered() || 
          !valid_event.has_triggered())
      {
        IndexSpaceExpression::TightenIndexSpaceArgs args(this);
        if (!this->realm_index_space_ready.has_triggered())
        {
          if (!valid_event.has_triggered())
            this->tight_index_space_ready = 
              ctx->runtime->issue_runtime_meta_task(args, 
                  LG_LATENCY_WORK_PRIORITY, Runtime::merge_events(valid_event,
                    Runtime::protect_event(this->realm_index_space_ready)));
          else
            this->tight_index_space_ready = 
              ctx->runtime->issue_runtime_meta_task(args, 
                  LG_LATENCY_WORK_PRIORITY,
                  Runtime::protect_event(this->realm_index_space_ready));
        }
        else
          this->tight_index_space_ready = 
            ctx->runtime->issue_runtime_meta_task(args, 
                LG_LATENCY_WORK_PRIORITY, valid_event);
      }
      else // We can do the tighten call now
        this->tighten_index_space();
      if (ctx->runtime->legion_spy_enabled)
      {
        std::vector<IndexSpaceExprID> sources(this->sub_expressions.size()); 
        for (unsigned idx = 0; idx < this->sub_expressions.size(); idx++)
          sources[idx] = this->sub_expressions[idx]->expr_id;
        LegionSpy::log_index_space_union(this->expr_id, sources);
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceUnion<DIM,T>::IndexSpaceUnion(
                            const std::vector<IndexSpaceExpression*> &to_union,
                            RegionTreeForest *ctx, Deserializer &derez)
      : IndexSpaceOperationT<DIM,T>(IndexSpaceOperation::UNION_OP_KIND, 
                                    ctx, derez), sub_expressions(to_union)
    //--------------------------------------------------------------------------
    {
      // Just update the tree correctly with references
      for (unsigned idx = 0; idx < sub_expressions.size(); idx++)
      {
        IndexSpaceExpression *sub = sub_expressions[idx];
        // Add the parent and the reference
        sub->add_parent_operation(this);
        sub->add_expression_reference();
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceUnion<DIM,T>::IndexSpaceUnion(const IndexSpaceUnion<DIM,T> &rhs)
      : IndexSpaceOperationT<DIM,T>(IndexSpaceOperation::UNION_OP_KIND, NULL)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceUnion<DIM,T>::~IndexSpaceUnion(void)
    //--------------------------------------------------------------------------
    {
      // Remove references from our sub expressions
      for (unsigned idx = 0; idx < sub_expressions.size(); idx++)
        if (sub_expressions[idx]->remove_expression_reference())
          delete sub_expressions[idx];
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceUnion<DIM,T>& IndexSpaceUnion<DIM,T>::operator=(
                                              const IndexSpaceUnion<DIM,T> &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceUnion<DIM,T>::pack_expression_structure(Serializer &rez,
                                                          AddressSpaceID target,
                                                          const bool top)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(target != this->context->runtime->address_space);
#endif
      if (top)
        this->record_remote_expression(target);
      rez.serialize<bool>(false); // not an index space
      if (target == this->origin_space)
      {
        rez.serialize<bool>(true); // local
        rez.serialize(this->origin_expr);
      }
      else
      {
        rez.serialize<bool>(false); // not local
        const bool local_empty = this->is_empty();
        if (!local_empty)
        {
          rez.serialize<size_t>(sub_expressions.size());
          for (std::vector<IndexSpaceExpression*>::const_iterator it = 
                sub_expressions.begin(); it != sub_expressions.end(); it++)
            (*it)->pack_expression_structure(rez, target, false/*top*/);
        }
        else
          rez.serialize<size_t>(0);
        rez.serialize(this->op_kind); 
        rez.serialize(this->type_tag); // unpacked by creator
        rez.serialize(this->expr_id); // unpacked by IndexSpaceOperation
        rez.serialize(this->origin_expr); // unpacked by IndexSpaceOperation
        // unpacked by IndexSpaceOperationT
        if (!local_empty)
        {
          Realm::IndexSpace<DIM,T> temp;
          ApEvent ready = this->get_realm_index_space(temp, true/*tight*/);
          rez.serialize(temp);
          rez.serialize(ready);
        }
        else
        {
          const Realm::IndexSpace<DIM,T> temp = 
            Realm::IndexSpace<DIM,T>::make_empty();
          rez.serialize(temp);
          rez.serialize(ApEvent::NO_AP_EVENT);
        }
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexSpaceUnion<DIM,T>::remove_operation(RegionTreeForest *forest)
    //--------------------------------------------------------------------------
    {
      // Remove the parent operation from all the sub expressions
      for (unsigned idx = 0; idx < sub_expressions.size(); idx++)
        sub_expressions[idx]->remove_parent_operation(this);
      // Then remove ourselves from the tree
      if (forest != NULL)
        forest->remove_union_operation(this, sub_expressions);
      // Remove our expression reference added by invalidate_operation
      // and return true if we should be deleted
      return this->remove_expression_reference();
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceExpression* IndexSpaceUnion<DIM,T>::find_congruence(void) 
    //--------------------------------------------------------------------------
    {
      const size_t local_volume = this->get_volume();
      if (local_volume == 0)
        return NULL;
      for (typename std::vector<IndexSpaceExpression*>::const_iterator it = 
            sub_expressions.begin(); it != sub_expressions.end(); it++)
        if ((*it)->get_volume() == local_volume)
          return (*it);
      return NULL;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceIntersection<DIM,T>::IndexSpaceIntersection(
                            const std::vector<IndexSpaceExpression*> &to_inter,
                            RegionTreeForest *ctx)
      : IndexSpaceOperationT<DIM,T>(IndexSpaceOperation::INTERSECT_OP_KIND,ctx),
        sub_expressions(to_inter)
    //--------------------------------------------------------------------------
    {
      std::set<ApEvent> preconditions;
      std::vector<Realm::IndexSpace<DIM,T> > spaces(sub_expressions.size());
      for (unsigned idx = 0; idx < sub_expressions.size(); idx++)
      {
        IndexSpaceExpression *sub = sub_expressions[idx];
        // Add the parent and the reference
        sub->add_parent_operation(this);
        sub->add_expression_reference();
        ApEvent precondition = sub->get_expr_index_space(
            &spaces[idx], this->type_tag, false/*need tight result*/);
        if (precondition.exists())
          preconditions.insert(precondition);
      }
      // Kick this off to Realm
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      Realm::ProfilingRequestSet requests;
      if (ctx->runtime->profiler != NULL)
        ctx->runtime->profiler->add_partition_request(requests,
                implicit_provenance, DEP_PART_INTERSECTION_REDUCTION);
      this->realm_index_space_ready = ApEvent(
          Realm::IndexSpace<DIM,T>::compute_intersection(
              spaces, this->realm_index_space, requests, precondition));
      // Then launch the tighten call for it too since we know we're
      // going to want this eventually
      const RtEvent valid_event(this->realm_index_space.make_valid());
      // See if both the events needed for the tighten call are done
      if (!this->realm_index_space_ready.has_triggered() || 
          !valid_event.has_triggered())
      {
        IndexSpaceExpression::TightenIndexSpaceArgs args(this);
        if (!this->realm_index_space_ready.has_triggered())
        {
          if (!valid_event.has_triggered())
            this->tight_index_space_ready = 
              ctx->runtime->issue_runtime_meta_task(args, 
                  LG_LATENCY_WORK_PRIORITY, Runtime::merge_events(valid_event,
                    Runtime::protect_event(this->realm_index_space_ready)));
          else
            this->tight_index_space_ready = 
              ctx->runtime->issue_runtime_meta_task(args, 
                  LG_LATENCY_WORK_PRIORITY,
                  Runtime::protect_event(this->realm_index_space_ready));
        }
        else
          this->tight_index_space_ready = 
            ctx->runtime->issue_runtime_meta_task(args, 
                LG_LATENCY_WORK_PRIORITY, valid_event);
      }
      else // We can do the tighten call now
        this->tighten_index_space();
      if (ctx->runtime->legion_spy_enabled)
      {
        std::vector<IndexSpaceExprID> sources(this->sub_expressions.size()); 
        for (unsigned idx = 0; idx < this->sub_expressions.size(); idx++)
          sources[idx] = this->sub_expressions[idx]->expr_id;
        LegionSpy::log_index_space_intersection(this->expr_id, sources);
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceIntersection<DIM,T>::IndexSpaceIntersection(
                            const std::vector<IndexSpaceExpression*> &to_inter,
                            RegionTreeForest *ctx, Deserializer &derez)
      : IndexSpaceOperationT<DIM,T>(IndexSpaceOperation::INTERSECT_OP_KIND,
                                    ctx, derez), sub_expressions(to_inter)
    //--------------------------------------------------------------------------
    {
      // Just update the tree correctly with references
      for (unsigned idx = 0; idx < sub_expressions.size(); idx++)
      {
        IndexSpaceExpression *sub = sub_expressions[idx];
        // Add the parent and the reference
        sub->add_parent_operation(this);
        sub->add_expression_reference();
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceIntersection<DIM,T>::IndexSpaceIntersection(
                                      const IndexSpaceIntersection<DIM,T> &rhs)
      : IndexSpaceOperationT<DIM,T>(IndexSpaceOperation::INTERSECT_OP_KIND,NULL)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceIntersection<DIM,T>::~IndexSpaceIntersection(void)
    //--------------------------------------------------------------------------
    {
      // Remove references from our sub expressions
      for (unsigned idx = 0; idx < sub_expressions.size(); idx++)
        if (sub_expressions[idx]->remove_expression_reference())
          delete sub_expressions[idx];
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceIntersection<DIM,T>& IndexSpaceIntersection<DIM,T>::operator=(
                                       const IndexSpaceIntersection<DIM,T> &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceIntersection<DIM,T>::pack_expression_structure(
                         Serializer &rez, AddressSpaceID target, const bool top)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(target != this->context->runtime->address_space);
#endif
      if (top)
        this->record_remote_expression(target);
      rez.serialize<bool>(false); // not an index space
      if (target == this->origin_space)
      {
        rez.serialize<bool>(true); // local
        rez.serialize(this->origin_expr);
      }
      else
      {
        rez.serialize<bool>(false); // not local
        const bool local_empty = this->is_empty();
        if (!local_empty)
        {
          rez.serialize<size_t>(sub_expressions.size());
          for (std::vector<IndexSpaceExpression*>::const_iterator it = 
                sub_expressions.begin(); it != sub_expressions.end(); it++)
            (*it)->pack_expression_structure(rez, target, false/*top*/);
        }
        else
          rez.serialize<size_t>(0);
        rez.serialize(this->op_kind); 
        rez.serialize(this->type_tag); // unpacked by creator
        rez.serialize(this->expr_id); // unpacked by IndexSpaceOperation
        rez.serialize(this->origin_expr); // unpacked by IndexSpaceOperation
        // unpacked by IndexSpaceOperationT
        if (!local_empty)
        {
          Realm::IndexSpace<DIM,T> temp;
          ApEvent ready = this->get_realm_index_space(temp, true/*tight*/);
          rez.serialize(temp);
          rez.serialize(ready);
        }
        else
        {
          const Realm::IndexSpace<DIM,T> temp = 
            Realm::IndexSpace<DIM,T>::make_empty();
          rez.serialize(temp);
          rez.serialize(ApEvent::NO_AP_EVENT);
        }
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexSpaceIntersection<DIM,T>::remove_operation(
                                                       RegionTreeForest *forest)
    //--------------------------------------------------------------------------
    {
      // Remove the parent operation from all the sub expressions
      for (unsigned idx = 0; idx < sub_expressions.size(); idx++)
        sub_expressions[idx]->remove_parent_operation(this);
      // Then remove ourselves from the tree
      if (forest != NULL)
        forest->remove_intersection_operation(this, sub_expressions);
      // Remove our expression reference added by invalidate_operation
      // and return true if we should be deleted
      return this->remove_expression_reference();
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceExpression* IndexSpaceIntersection<DIM,T>::find_congruence(void)
    //--------------------------------------------------------------------------
    {
      const size_t local_volume = this->get_volume();
      if (local_volume == 0)
        return NULL;
      for (typename std::vector<IndexSpaceExpression*>::const_iterator it = 
            sub_expressions.begin(); it != sub_expressions.end(); it++)
        if ((*it)->get_volume() == local_volume)
          return (*it);
      return NULL;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceDifference<DIM,T>::IndexSpaceDifference(IndexSpaceExpression *l,
                IndexSpaceExpression *r, RegionTreeForest *ctx) 
      : IndexSpaceOperationT<DIM,T>(IndexSpaceOperation::DIFFERENCE_OP_KIND,ctx)
        , lhs(l), rhs(r)
    //--------------------------------------------------------------------------
    {
      if (lhs == rhs)
      {
        // Special case for when the expressions are the same
        lhs->add_parent_operation(this);
        lhs->add_expression_reference();
        this->realm_index_space = Realm::IndexSpace<DIM,T>::make_empty();
        this->tight_index_space = Realm::IndexSpace<DIM,T>::make_empty();
        this->realm_index_space_ready = ApEvent::NO_AP_EVENT;
        this->tight_index_space_ready = RtEvent::NO_RT_EVENT;
      }
      else
      {
        Realm::IndexSpace<DIM,T> lhs_space, rhs_space;
        // Add the parent and the references
        lhs->add_parent_operation(this);
        rhs->add_parent_operation(this);
        lhs->add_expression_reference();
        rhs->add_expression_reference();
        ApEvent left_ready = 
          lhs->get_expr_index_space(&lhs_space, this->type_tag, false/*tight*/);
        ApEvent right_ready = 
          rhs->get_expr_index_space(&rhs_space, this->type_tag, false/*tight*/);
        ApEvent precondition = 
          Runtime::merge_events(NULL, left_ready, right_ready);
        Realm::ProfilingRequestSet requests;
        if (ctx->runtime->profiler != NULL)
          ctx->runtime->profiler->add_partition_request(requests,
                                implicit_provenance, DEP_PART_DIFFERENCE);
        this->realm_index_space_ready = ApEvent(
            Realm::IndexSpace<DIM,T>::compute_difference(lhs_space, rhs_space, 
                              this->realm_index_space, requests, precondition));
        // Then launch the tighten call for it too since we know we're
        // going to want this eventually
        const RtEvent valid_event(this->realm_index_space.make_valid());
        // See if both the events needed for the tighten call are done
        if (!this->realm_index_space_ready.has_triggered() || 
            !valid_event.has_triggered())
        {
          IndexSpaceExpression::TightenIndexSpaceArgs args(this);
          if (!this->realm_index_space_ready.has_triggered())
          {
            if (!valid_event.has_triggered())
              this->tight_index_space_ready = 
                ctx->runtime->issue_runtime_meta_task(args, 
                    LG_LATENCY_WORK_PRIORITY, Runtime::merge_events(valid_event,
                      Runtime::protect_event(this->realm_index_space_ready)));
            else
              this->tight_index_space_ready = 
                ctx->runtime->issue_runtime_meta_task(args, 
                    LG_LATENCY_WORK_PRIORITY,
                    Runtime::protect_event(this->realm_index_space_ready));
          }
          else
            this->tight_index_space_ready = 
              ctx->runtime->issue_runtime_meta_task(args, 
                  LG_LATENCY_WORK_PRIORITY, valid_event);
        }
        else // We can do the tighten call now
          this->tighten_index_space();
      }
      if (ctx->runtime->legion_spy_enabled)
        LegionSpy::log_index_space_difference(this->expr_id,
                                              lhs->expr_id, rhs->expr_id);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceDifference<DIM,T>::IndexSpaceDifference(IndexSpaceExpression *l,
            IndexSpaceExpression *r, RegionTreeForest *ctx, Deserializer &derez) 
      : IndexSpaceOperationT<DIM,T>(IndexSpaceOperation::DIFFERENCE_OP_KIND,
                                    ctx, derez), lhs(l), rhs(r)
    //--------------------------------------------------------------------------
    {
      if (lhs != NULL)
      {
        lhs->add_parent_operation(this);
        lhs->add_expression_reference();
      }
      if (rhs != NULL)
      {
        rhs->add_parent_operation(this);
        rhs->add_expression_reference();
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceDifference<DIM,T>::IndexSpaceDifference(
                                      const IndexSpaceDifference<DIM,T> &rhs)
     : IndexSpaceOperationT<DIM,T>(IndexSpaceOperation::DIFFERENCE_OP_KIND,
                                   NULL), lhs(NULL), rhs(NULL)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceDifference<DIM,T>::~IndexSpaceDifference(void)
    //--------------------------------------------------------------------------
    {
      if ((rhs != NULL) && (lhs != rhs) && rhs->remove_expression_reference())
        delete rhs;
      if ((lhs != NULL) && lhs->remove_expression_reference())
        delete lhs;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceDifference<DIM,T>& IndexSpaceDifference<DIM,T>::operator=(
                                         const IndexSpaceDifference<DIM,T> &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceDifference<DIM,T>::pack_expression_structure(Serializer &rez,
                                          AddressSpaceID target, const bool top)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(target != this->context->runtime->address_space);
#endif
      if (top)
        this->record_remote_expression(target);
      rez.serialize<bool>(false); // not an index space
      if (target == this->origin_space)
      {
        rez.serialize<bool>(true); // local
        rez.serialize(this->origin_expr);
      }
      else
      {
        rez.serialize<bool>(false); // not local
        const bool local_empty = this->is_empty();
        if (!local_empty)
        {
          rez.serialize<size_t>(2);
          lhs->pack_expression_structure(rez, target, false/*top*/);
          rhs->pack_expression_structure(rez, target, false/*top*/);
        }
        else
          rez.serialize<size_t>(0);
        rez.serialize(this->op_kind); 
        rez.serialize(this->type_tag); // unpacked by creator
        rez.serialize(this->expr_id); // unpacked by IndexSpaceOperation
        rez.serialize(this->origin_expr); // unpacked by IndexSpaceOperation
        // unpacked by IndexSpaceOperationT
        if (!local_empty)
        {
          Realm::IndexSpace<DIM,T> temp;
          ApEvent ready = this->get_realm_index_space(temp, true/*tight*/);
          rez.serialize(temp);
          rez.serialize(ready);
        }
        else
        {
          const Realm::IndexSpace<DIM,T> temp = 
            Realm::IndexSpace<DIM,T>::make_empty();
          rez.serialize(temp);
          rez.serialize(ApEvent::NO_AP_EVENT);
        }
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexSpaceDifference<DIM,T>::remove_operation(RegionTreeForest *forest)
    //--------------------------------------------------------------------------
    {
      // Remove the parent operation from all the sub expressions
      if (lhs != NULL)
        lhs->remove_parent_operation(this);
      if ((rhs != NULL) && (lhs != rhs))
        rhs->remove_parent_operation(this);
      // Then remove ourselves from the tree
      if ((forest != NULL) && (lhs != NULL) && (rhs != NULL))
        forest->remove_subtraction_operation(this, lhs, rhs);
      // Remove our expression reference added by invalidate_operation
      // and return true if we should be deleted
      return this->remove_expression_reference();
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceExpression* IndexSpaceDifference<DIM,T>::find_congruence(void)
    //--------------------------------------------------------------------------
    {
      const size_t local_volume = this->get_volume();
      if (local_volume == 0)
        return NULL;
      if (local_volume == lhs->get_volume())
        return lhs;
      return NULL;
    }

    /////////////////////////////////////////////////////////////
    // Templated Index Space Node 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceNodeT<DIM,T>::IndexSpaceNodeT(RegionTreeForest *ctx, 
        IndexSpace handle, IndexPartNode *parent, LegionColor color,
        const Realm::IndexSpace<DIM,T> *is, DistributedID did, 
        ApEvent ready, IndexSpaceExprID expr_id)
      : IndexSpaceNode(ctx, handle, parent, color, did, ready, expr_id), 
        linearization_ready(false)
    //--------------------------------------------------------------------------
    {
      if (is != NULL)
      {
        realm_index_space = *is;
        Runtime::trigger_event(realm_index_space_set);
      }
      else
        add_base_resource_ref(RUNTIME_REF);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceNodeT<DIM,T>::IndexSpaceNodeT(const IndexSpaceNodeT &rhs)
      : IndexSpaceNode(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceNodeT<DIM,T>::~IndexSpaceNodeT(void)
    //--------------------------------------------------------------------------
    { 
      // Destory our index space if we are the owner and it has been set
      if (is_owner() && realm_index_space_set.has_triggered())
        realm_index_space.destroy();
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexSpaceNodeT<DIM,T>& IndexSpaceNodeT<DIM,T>::operator=(
                                                     const IndexSpaceNodeT &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::get_realm_index_space(
                      Realm::IndexSpace<DIM,T> &result, bool need_tight_result)
    //--------------------------------------------------------------------------
    {
      if (!tight_index_space)
      {
        if (need_tight_result)
        {
          // Wait for the index space to be tight
          tight_index_space_set.wait();
          // Fall through and get the result when we're done
        }
        else
        {
          if (!realm_index_space_set.has_triggered())
            realm_index_space_set.wait();
          // Not tight yet so still subject to change so we need the lock
          AutoLock n_lock(node_lock,1,false/*exclusive*/);
          result = realm_index_space;
          return index_space_ready;
        }
      }
      // At this point we have a tight index space
      // That means it's already ready
      result = realm_index_space;
      return ApEvent::NO_AP_EVENT;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexSpaceNodeT<DIM,T>::set_realm_index_space(
                  AddressSpaceID source, const Realm::IndexSpace<DIM,T> &value)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!realm_index_space_set.has_triggered());
#endif
      // We can set this now but triggering the realm_index_space_set
      // event has to be done while holding the node_lock on the owner
      // node so that it is serialized with respect to queries from 
      // remote nodes for copies about the remote instance
      realm_index_space = value;
      // If we're not the owner, send a message back to the
      // owner specifying that it can set the index space value
      const AddressSpaceID owner_space = get_owner_space();
      if (owner_space != context->runtime->address_space)
      {
        // We're not the owner so we can trigger the event without the lock
        Runtime::trigger_event(realm_index_space_set);
        // We're not the owner, if this is not from the owner then
        // send a message there telling the owner that it is set
        if (source != owner_space)
        {
          Serializer rez;
          {
            RezCheck z(rez);
            rez.serialize(handle);
            pack_index_space(rez, false/*include size*/);
          }
          context->runtime->send_index_space_set(owner_space, rez);
        }
      }
      else
      {
        // Hold the lock while walking over the node set
        AutoLock n_lock(node_lock);
        // Now we can trigger the event while holding the lock
        Runtime::trigger_event(realm_index_space_set);
        if (has_remote_instances())
        {
          // We're the owner, send messages to everyone else that we've 
          // sent this node to except the source
          Serializer rez;
          {
            RezCheck z(rez);
            rez.serialize(handle);
            pack_index_space(rez, false/*include size*/);
          }
          IndexSpaceSetFunctor functor(context->runtime, source, rez);
          map_over_remote_instances(functor);
        }
      }
      // Now we can tighten it
      tighten_index_space();
      // Remove the reference we were holding until this was set
      return remove_base_resource_ref(RUNTIME_REF);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::get_expr_index_space(void *result,
                                            TypeTag tag, bool need_tight_result)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(type_tag == handle.get_type_tag());
#endif
      Realm::IndexSpace<DIM,T> *space = 
        reinterpret_cast<Realm::IndexSpace<DIM,T>*>(result);
      return get_realm_index_space(*space, need_tight_result);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    Domain IndexSpaceNodeT<DIM,T>::get_domain(ApEvent &ready, bool need_tight)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> result;
      ready = get_realm_index_space(result, need_tight);
      return DomainT<DIM,T>(result);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::tighten_index_space(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!tight_index_space);
      assert(!tight_index_space_set.has_triggered());
#endif
      const RtEvent valid_event(realm_index_space.make_valid());
      if (!index_space_ready.has_triggered() || !valid_event.has_triggered())
      {
        // If this index space isn't ready yet, then we have to defer this 
        TightenIndexSpaceArgs args(this);
        if (!index_space_ready.has_triggered())
        {
          if (!valid_event.has_triggered())
            context->runtime->issue_runtime_meta_task(args,
                LG_LATENCY_WORK_PRIORITY, Runtime::merge_events(valid_event,
                  Runtime::protect_event(index_space_ready)));
          else
            context->runtime->issue_runtime_meta_task(args,
                LG_LATENCY_WORK_PRIORITY,
                Runtime::protect_event(index_space_ready));
        }
        else
          context->runtime->issue_runtime_meta_task(args,
              LG_LATENCY_WORK_PRIORITY, valid_event);
        
        return;
      }
#ifdef DEBUG_LEGION
      assert(realm_index_space.is_valid());
#endif
      Realm::IndexSpace<DIM,T> tight_space = realm_index_space.tighten();
#ifdef DEBUG_LEGION
      assert(tight_space.is_valid());
#endif
      Realm::IndexSpace<DIM,T> old_space;
      // Now take the lock and set everything
      {
        AutoLock n_lock(node_lock);
        old_space = realm_index_space;
        realm_index_space = tight_space;
        __sync_synchronize(); // small memory fence to propagate writes
        tight_index_space = true;
      }
      Runtime::trigger_event(tight_index_space_set);
      old_space.destroy();
      if (context->runtime->legion_spy_enabled || 
          (context->runtime->profiler != NULL))
      {
        // Log subspaces being set on the owner
        const AddressSpaceID owner_space = get_owner_space();
        if (owner_space == context->runtime->address_space)
        {
          if (context->runtime->legion_spy_enabled)
            this->log_index_space_points(tight_space);
          if (context->runtime->profiler != NULL)
            this->log_profiler_index_space_points(tight_space);
        }
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexSpaceNodeT<DIM,T>::check_empty(void)
    //--------------------------------------------------------------------------
    {
      return (get_volume() == 0);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::pack_expression(Serializer &rez, 
                                                 AddressSpaceID target)
    //--------------------------------------------------------------------------
    {
      if (target != context->runtime->address_space)
      {
        rez.serialize<bool>(false/*local*/);
        rez.serialize<bool>(true/*index space*/);
        rez.serialize(handle);
      }
      else
      {
        rez.serialize<bool>(true/*local*/);
        rez.serialize<IndexSpaceExpression*>(this);
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::pack_expression_structure(Serializer &rez,
                                                          AddressSpaceID target,
                                                          const bool top) 
    //--------------------------------------------------------------------------
    {
      rez.serialize<bool>(true/*index space*/);
      rez.serialize(handle);
      // Make sure this doesn't get collected until we're unpacked
      // This could be a performance bug since it will block if we
      // have to send a reference to a remote node, but that should
      // never actually happen
      LocalReferenceMutator mutator;
      add_base_gc_ref(REMOTE_DID_REF, &mutator);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::log_index_space_points(
                              const Realm::IndexSpace<DIM,T> &tight_space) const
    //--------------------------------------------------------------------------
    {
      // Be careful, Realm can lie to us here
      if (!tight_space.empty())
      {
        bool logged = false;
        // Iterate over the rectangles and print them out 
        for (Realm::IndexSpaceIterator<DIM,T> itr(tight_space); 
              itr.valid; itr.step())
        {
          const size_t rect_volume = itr.rect.volume();
          if (rect_volume == 0)
            continue;
          logged = true;
          if (rect_volume == 1)
            LegionSpy::log_index_space_point(handle.get_id(), 
                                             Point<DIM,T>(itr.rect.lo));
          else
            LegionSpy::log_index_space_rect(handle.get_id(), 
                                            Rect<DIM,T>(itr.rect));
        }
        // Handle the case where Realm lied to us about being empty
        if (!logged)
          LegionSpy::log_empty_index_space(handle.get_id());
      }
      else
        LegionSpy::log_empty_index_space(handle.get_id());
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::log_profiler_index_space_points(
                              const Realm::IndexSpace<DIM,T> &tight_space) const
    //--------------------------------------------------------------------------
    {
      if (!tight_space.empty())
      {
        // Iterate over the rectangles and print them out
        for (Realm::IndexSpaceIterator<DIM,T> itr(tight_space);
              itr.valid; itr.step())
        {
          if (itr.rect.volume() == 1)
            context->runtime->profiler->record_index_space_point(
                handle.get_id(), Point<DIM,T>(itr.rect.lo));
          else
            context->runtime->profiler->record_index_space_rect(
                handle.get_id(), Rect<DIM,T>(itr.rect));
        }
      }
      else
        context->runtime->profiler->record_empty_index_space(handle.get_id());
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::compute_pending_space(Operation *op,
                          const std::vector<IndexSpace> &handles, bool is_union)
    //--------------------------------------------------------------------------
    {
      std::set<ApEvent> preconditions;
      std::vector<Realm::IndexSpace<DIM,T> > spaces(handles.size());
      for (unsigned idx = 0; idx < handles.size(); idx++)
      {
        IndexSpaceNode *node = context->get_node(handles[idx]);
        if (handles[idx].get_type_tag() != handle.get_type_tag())
        {
          TaskContext *ctx = op->get_context();
          if (is_union)
            REPORT_LEGION_ERROR(ERROR_DYNAMIC_TYPE_MISMATCH,
                          "Dynamic type mismatch in 'create_index_space_union' "
                          "performed in task %s (UID %lld)",
                          ctx->get_task_name(), ctx->get_unique_id())
          else
            REPORT_LEGION_ERROR(ERROR_DYNAMIC_TYPE_MISMATCH,
                          "Dynamic type mismatch in "
                          "'create_index_space_intersection' performed in "
                          "task %s (UID %lld)", ctx->get_task_name(),
                          ctx->get_unique_id())
        }
        IndexSpaceNodeT<DIM,T> *space = 
          static_cast<IndexSpaceNodeT<DIM,T>*>(node);
        ApEvent ready = space->get_realm_index_space(spaces[idx], false);
        if (ready.exists())
          preconditions.insert(ready);
      }
      if (op->has_execution_fence_event())
        preconditions.insert(op->get_execution_fence_event());
      // Kick this off to Realm
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      Realm::IndexSpace<DIM,T> result_space;
      if (is_union)
      {
        Realm::ProfilingRequestSet requests;
        if (context->runtime->profiler != NULL)
          context->runtime->profiler->add_partition_request(requests,
                                        op, DEP_PART_UNION_REDUCTION);
        ApEvent result(Realm::IndexSpace<DIM,T>::compute_union(
              spaces, result_space, requests, precondition));
        if (set_realm_index_space(context->runtime->address_space,result_space))
          assert(false); // should never hit this
        return result;
      }
      else
      {
        Realm::ProfilingRequestSet requests;
        if (context->runtime->profiler != NULL)
          context->runtime->profiler->add_partition_request(requests,
                                op, DEP_PART_INTERSECTION_REDUCTION);
        ApEvent result(Realm::IndexSpace<DIM,T>::compute_intersection(
              spaces, result_space, requests, precondition));
        if (set_realm_index_space(context->runtime->address_space,result_space))
          assert(false); // should never hit this
        return result;
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::compute_pending_space(Operation *op,
                                      IndexPartition part_handle, bool is_union)
    //--------------------------------------------------------------------------
    {
      if (part_handle.get_type_tag() != handle.get_type_tag())
      {
        TaskContext *ctx = op->get_context();
        if (is_union)
          REPORT_LEGION_ERROR(ERROR_DYNAMIC_TYPE_MISMATCH,
                        "Dynamic type mismatch in 'create_index_space_union' "
                        "performed in task %s (UID %lld)",
                        ctx->get_task_name(), ctx->get_unique_id())
        else
          REPORT_LEGION_ERROR(ERROR_DYNAMIC_TYPE_MISMATCH,
                        "Dynamic type mismatch in "
                        "'create_index_space_intersection' performed in "
                        "task %s (UID %lld)", ctx->get_task_name(),
                        ctx->get_unique_id())
      }
      IndexPartNode *partition = context->get_node(part_handle);
      std::set<ApEvent> preconditions;
      std::vector<Realm::IndexSpace<DIM,T> > 
        spaces(partition->color_space->get_volume());
      unsigned subspace_index = 0;
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          IndexSpaceNodeT<DIM,T> *child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
          ApEvent ready = child->get_realm_index_space(spaces[subspace_index++],
                                                       false/*tight*/);
          if (ready.exists())
            preconditions.insert(ready);
        }
      }
      else
      {
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          IndexSpaceNodeT<DIM,T> *child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
          ApEvent ready = child->get_realm_index_space(spaces[subspace_index++],
                                                       false/*tight*/);
          if (ready.exists())
            preconditions.insert(ready);
        }
        delete itr;
      }
      if (op->has_execution_fence_event())
        preconditions.insert(op->get_execution_fence_event());
      // Kick this off to Realm
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      Realm::IndexSpace<DIM,T> result_space;
      if (is_union)
      {
        Realm::ProfilingRequestSet requests;
        if (context->runtime->profiler != NULL)
          context->runtime->profiler->add_partition_request(requests,
                                        op, DEP_PART_UNION_REDUCTION);
        ApEvent result(Realm::IndexSpace<DIM,T>::compute_union(
              spaces, result_space, requests, precondition));
        if (set_realm_index_space(context->runtime->address_space,result_space))
          assert(false); // should never hit this
        return result;
      }
      else
      {
        Realm::ProfilingRequestSet requests;
        if (context->runtime->profiler != NULL)
          context->runtime->profiler->add_partition_request(requests,
                                op, DEP_PART_INTERSECTION_REDUCTION);
        ApEvent result(Realm::IndexSpace<DIM,T>::compute_intersection(
              spaces, result_space, requests, precondition));
        if (set_realm_index_space(context->runtime->address_space,result_space))
          assert(false); // should never hit this
        return result;
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::compute_pending_difference(Operation *op,
                        IndexSpace init, const std::vector<IndexSpace> &handles)
    //--------------------------------------------------------------------------
    {
      if (init.get_type_tag() != handle.get_type_tag())
      {
        TaskContext *ctx = op->get_context();
        REPORT_LEGION_ERROR(ERROR_DYNAMIC_TYPE_MISMATCH,
                      "Dynamic type mismatch in "
                      "'create_index_space_difference' performed in "
                      "task %s (%lld)", ctx->get_task_name(), 
                      ctx->get_unique_id())
      }
      std::set<ApEvent> preconditions;
      std::vector<Realm::IndexSpace<DIM,T> > spaces(handles.size());
      for (unsigned idx = 0; idx < handles.size(); idx++)
      {
        IndexSpaceNode *node = context->get_node(handles[idx]);
        if (handles[idx].get_type_tag() != handle.get_type_tag())
        {
          TaskContext *ctx = op->get_context();
          REPORT_LEGION_ERROR(ERROR_DYNAMIC_TYPE_MISMATCH,
                        "Dynamic type mismatch in "
                        "'create_index_space_difference' performed in "
                        "task %s (%lld)", ctx->get_task_name(), 
                        ctx->get_unique_id())
        }
        IndexSpaceNodeT<DIM,T> *space = 
          static_cast<IndexSpaceNodeT<DIM,T>*>(node);
        ApEvent ready = space->get_realm_index_space(spaces[idx], 
                                                     false/*tight*/);
        if (ready.exists())
          preconditions.insert(ready);
      } 
      if (op->has_execution_fence_event())
        preconditions.insert(op->get_execution_fence_event());
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      Realm::ProfilingRequestSet union_requests;
      Realm::ProfilingRequestSet diff_requests;
      if (context->runtime->profiler != NULL)
      {
        context->runtime->profiler->add_partition_request(union_requests,
                                            op, DEP_PART_UNION_REDUCTION);
        context->runtime->profiler->add_partition_request(diff_requests,
                                            op, DEP_PART_DIFFERENCE);
      }
      // Compute the union of the handles for the right-hand side
      Realm::IndexSpace<DIM,T> rhs_space;
      ApEvent rhs_ready(Realm::IndexSpace<DIM,T>::compute_union(
            spaces, rhs_space, union_requests, precondition));
      IndexSpaceNodeT<DIM,T> *lhs_node = 
        static_cast<IndexSpaceNodeT<DIM,T>*>(context->get_node(init));
      Realm::IndexSpace<DIM,T> lhs_space, result_space;
      ApEvent lhs_ready = lhs_node->get_realm_index_space(lhs_space, false);
      ApEvent result(Realm::IndexSpace<DIM,T>::compute_difference(
            lhs_space, rhs_space, result_space, diff_requests,
            Runtime::merge_events(NULL, lhs_ready, rhs_ready)));
      if (set_realm_index_space(context->runtime->address_space, result_space))
        assert(false); // should never hit this
      // Destroy the tempory rhs space once the computation is done
      rhs_space.destroy(result);
      return result;
    } 

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::get_index_space_domain(void *realm_is, 
                                                        TypeTag type_tag)
    //--------------------------------------------------------------------------
    {
      if (type_tag != handle.get_type_tag())
        REPORT_LEGION_ERROR(ERROR_DYNAMIC_TYPE_MISMATCH,
            "Dynamic type mismatch in 'get_index_space_domain'")
      Realm::IndexSpace<DIM,T> *target = 
        static_cast<Realm::IndexSpace<DIM,T>*>(realm_is);
      // No need to wait since we're waiting for it to be tight
      // which implies that it will be ready
      get_realm_index_space(*target, true/*tight*/);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    size_t IndexSpaceNodeT<DIM,T>::get_volume(void)
    //--------------------------------------------------------------------------
    {
      if (has_volume)
        return volume;
      Realm::IndexSpace<DIM,T> volume_space;
      get_realm_index_space(volume_space, true/*tight*/);
      volume = volume_space.volume();
      __sync_synchronize();
      has_volume = true;
      return volume;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    size_t IndexSpaceNodeT<DIM,T>::get_num_dims(void) const
    //--------------------------------------------------------------------------
    {
      return DIM;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexSpaceNodeT<DIM,T>::contains_point(const void *realm_point, 
                                                TypeTag type_tag)
    //--------------------------------------------------------------------------
    {
      if (type_tag != handle.get_type_tag())
        REPORT_LEGION_ERROR(ERROR_DYNAMIC_TYPE_MISMATCH,
            "Dynamic type mismatch in 'safe_cast'")
      const Realm::Point<DIM,T> *point = 
        static_cast<const Realm::Point<DIM,T>*>(realm_point);
      Realm::IndexSpace<DIM,T> test_space;
      // Wait for a tight space on which to perform the test
      get_realm_index_space(test_space, true/*tight*/);
      return test_space.contains(*point);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexSpaceNodeT<DIM,T>::contains_point(const Realm::Point<DIM,T> &p)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> test_space;
      // Wait for a tight space on which to perform the test
      get_realm_index_space(test_space, true/*tight*/);
      return test_space.contains(p);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexSpaceNodeT<DIM,T>::destroy_node(AddressSpaceID source,
                                              std::set<RtEvent> &applied)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(registered_with_runtime);
#endif
      if (destroyed)
        REPORT_LEGION_ERROR(ERROR_ILLEGAL_INDEX_SPACE_DELETION,
            "Duplicate deletion of Index Space %d", handle.get_id())
      destroyed = true;
      // If we're not the owner, send a message that we're removing
      // the application reference
      if (!is_owner())
      {
        if (source != owner_space)
          runtime->send_index_space_destruction(handle, owner_space, applied);
        return false;
      }
      else
      {
        if (has_remote_instances())
        {
          DestroyNodeFunctor functor(handle, source, runtime, applied);
          map_over_remote_instances(functor);
        }
        // Traverse down and destroy all of the child nodes
        // Need to make a copy of this in case the children
        // end up being deleted and removing themselves
        std::vector<IndexPartNode*> color_map_copy;
        {
          unsigned index = 0;
          AutoLock n_lock(node_lock,1,false/*exclusive*/);
          if (!color_map.empty())
          {
            color_map_copy.resize(color_map.size());
            for (std::map<LegionColor,IndexPartNode*>::const_iterator it = 
                  color_map.begin(); it != color_map.end(); it++)
              color_map_copy[index++] = it->second;
          }
        }
        if (!color_map_copy.empty())
        {
          for (std::vector<IndexPartNode*>::const_iterator it = 
                color_map_copy.begin(); it != color_map_copy.end(); it++)
            if ((*it)->destroy_node(local_space, false/*top*/, applied))
              delete (*it);
        }
        return remove_base_valid_ref(APPLICATION_REF, NULL/*mutator*/);
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    LegionColor IndexSpaceNodeT<DIM,T>::get_max_linearized_color(void)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> color_bounds;
      get_realm_index_space(color_bounds, true/*tight*/);
      return color_bounds.bounds.volume();
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::compute_linearization_metadata(void)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> space;
      get_realm_index_space(space, true/*tight*/);
      // Don't need to wait for full index space since we just need bounds
      const Realm::Rect<DIM,T> &bounds = space.bounds;
      const long long volume = bounds.volume();
      if (volume > 0)
      {
        long long stride = 1;
        for (int idx = 0; idx < DIM; idx++)
        {
          offset[idx] = bounds.lo[idx];
          strides[idx] = stride;
          stride *= ((bounds.hi[idx] - bounds.lo[idx]) + 1);
        }
#ifdef DEBUG_LEGION
        assert(stride == volume);
#endif
      }
      else
      {
        for (int idx = 0; idx < DIM; idx++)
        {
          offset[idx] = 0;
          strides[idx] = 0;
        }
      }
      // Need a memory fence here to make sure that writes propagate on 
      // non-total-store-ordered memory consistency machines like PowerPC
      __sync_synchronize();
      linearization_ready = true;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    LegionColor IndexSpaceNodeT<DIM,T>::linearize_color(const void *realm_color,
                                                        TypeTag type_tag)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(type_tag == handle.get_type_tag());
#endif
      if (!linearization_ready)
        compute_linearization_metadata();
      Realm::Point<DIM,T> point = 
        *(static_cast<const Realm::Point<DIM,T>*>(realm_color));
      // First subtract the offset to get to the origin
      point -= offset;
      LegionColor color = 0;
      for (int idx = 0; idx < DIM; idx++)
        color += point[idx] * strides[idx];
      return color;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    LegionColor IndexSpaceNodeT<DIM,T>::linearize_color(Point<DIM,T> point)
    //--------------------------------------------------------------------------
    {
      if (!linearization_ready)
        compute_linearization_metadata();
      // First subtract the offset to get to the origin
      point -= offset;
      LegionColor color = 0;
      for (int idx = 0; idx < DIM; idx++)
        color += point[idx] * strides[idx];
      return color;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::delinearize_color(LegionColor color,
                                            void *realm_color, TypeTag type_tag)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(type_tag == handle.get_type_tag());
#endif
      if (!linearization_ready)
        compute_linearization_metadata();
      Realm::Point<DIM,T> &point = 
        *(static_cast<Realm::Point<DIM,T>*>(realm_color));
      for (int idx = DIM-1; idx >= 0; idx--)
      {
        point[idx] = color/strides[idx]; // truncates
        color -= point[idx] * strides[idx];
      }
      point += offset;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ColorSpaceIterator* 
                       IndexSpaceNodeT<DIM,T>::create_color_space_iterator(void)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> color_space;
      // Wait for a tight space on which to perform the test
      get_realm_index_space(color_space, true/*tight*/); 
      return new ColorSpaceIteratorT<DIM,T>(color_space, this);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexSpaceNodeT<DIM,T>::contains_color(LegionColor color, 
                                                bool report_error/*=false*/)
    //--------------------------------------------------------------------------
    {
      Realm::Point<DIM,T> point;
      delinearize_color(color, &point, handle.get_type_tag());
      Realm::IndexSpace<DIM,T> space;
      get_realm_index_space(space, true/*tight*/);
      if (!space.contains(point))
      {
        if (report_error)
          REPORT_LEGION_ERROR(ERROR_INVALID_INDEX_SPACE_COLOR,
              "Invalid color request")
        return false;
      }
      else
        return true;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::instantiate_colors(
                                               std::vector<LegionColor> &colors)
    //--------------------------------------------------------------------------
    {
      colors.resize(get_volume());
      unsigned idx = 0;
      Realm::IndexSpace<DIM,T> space;
      get_realm_index_space(space, true/*tight*/);
      for (Realm::IndexSpaceIterator<DIM,T> rect_itr(space); 
            rect_itr.valid; rect_itr.step())
      {
        for (Realm::PointInRectIterator<DIM,T> itr(rect_itr.rect);
              itr.valid; itr.step(), idx++)
          colors[idx] = linearize_color(&itr.p, handle.get_type_tag());
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    Domain IndexSpaceNodeT<DIM,T>::get_color_space_domain(void)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> space;
      get_realm_index_space(space, true/*tight*/);
      return Domain(DomainT<DIM,T>(space));
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    DomainPoint IndexSpaceNodeT<DIM,T>::get_domain_point_color(void) const
    //--------------------------------------------------------------------------
    {
      if (parent == NULL)
        return DomainPoint(color);
      return parent->color_space->delinearize_color_to_point(color); 
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    DomainPoint IndexSpaceNodeT<DIM,T>::delinearize_color_to_point(
                                                                  LegionColor c)
    //--------------------------------------------------------------------------
    {
      Realm::Point<DIM,T> color_point;
      delinearize_color(c, &color_point, handle.get_type_tag());
      return DomainPoint(Point<DIM,T>(color_point));
    } 

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::pack_index_space(Serializer &rez,
                                                  bool include_size) const
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(realm_index_space_set.has_triggered());
#endif
      if (include_size)
        rez.serialize<size_t>(sizeof(realm_index_space));
      // No need for the lock, held by the caller
      rez.serialize(realm_index_space);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexSpaceNodeT<DIM,T>::unpack_index_space(Deserializer &derez,
                                                    AddressSpaceID source)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> result_space;
      derez.deserialize(result_space);
      return set_realm_index_space(source, result_space);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_equal_children(Operation *op,
                                   IndexPartNode *partition, size_t granularity)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(partition->parent == this);
#endif
      const size_t count = partition->color_space->get_volume(); 
      std::vector<Realm::IndexSpace<DIM,T> > subspaces;
      Realm::ProfilingRequestSet requests;
      if (context->runtime->profiler != NULL)
        context->runtime->profiler->add_partition_request(requests,
                                                op, DEP_PART_EQUAL);
      Realm::IndexSpace<DIM,T> local_space;
      ApEvent ready = get_realm_index_space(local_space, false/*tight*/);
      if (op->has_execution_fence_event())
        ready = Runtime::merge_events(NULL, ready, 
                  op->get_execution_fence_event());
      ApEvent result(local_space.create_equal_subspaces(count, 
            granularity, subspaces, requests, ready));
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists() || (result == ready))
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      LegionSpy::log_deppart_events(op->get_unique_op_id(),handle,ready,result);
#endif
      // Enumerate the colors and assign the spaces
      unsigned subspace_index = 0;
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          IndexSpaceNodeT<DIM,T> *child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[subspace_index++]))
            assert(false); // should never hit this
        }
      }
      else
      {
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color(); 
          IndexSpaceNodeT<DIM,T> *child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[subspace_index++]))
            assert(false); // should never hit this
        }
        delete itr;
      }
      return result;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_by_union(Operation *op,
                                                    IndexPartNode *partition,
                                                    IndexPartNode *left,
                                                    IndexPartNode *right)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(partition->parent == this);
#endif
      const size_t count = partition->color_space->get_volume();
      std::vector<Realm::IndexSpace<DIM,T> > lhs_spaces(count);
      std::vector<Realm::IndexSpace<DIM,T> > rhs_spaces(count);
      std::set<ApEvent> preconditions;
      // First we need to fill in all the subspaces
      unsigned subspace_index = 0;
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          IndexSpaceNodeT<DIM,T> *left_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(left->get_child(color));
          IndexSpaceNodeT<DIM,T> *right_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(right->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < count);
#endif
          ApEvent left_ready = 
            left_child->get_realm_index_space(lhs_spaces[subspace_index],
                                              false/*tight*/);
          ApEvent right_ready = 
            right_child->get_realm_index_space(rhs_spaces[subspace_index++],
                                               false/*tight*/);
          if (left_ready.exists())
            preconditions.insert(left_ready);
          if (right_ready.exists())
            preconditions.insert(right_ready);
        }
      }
      else
      {
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          IndexSpaceNodeT<DIM,T> *left_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
          IndexSpaceNodeT<DIM,T> *right_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(right->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < count);
#endif
          ApEvent left_ready = 
            left_child->get_realm_index_space(lhs_spaces[subspace_index],
                                              false/*tight*/);
          ApEvent right_ready = 
            right_child->get_realm_index_space(rhs_spaces[subspace_index++],
                                               false/*tight*/);
          if (left_ready.exists())
            preconditions.insert(left_ready);
          if (right_ready.exists())
            preconditions.insert(right_ready);
        }
        delete itr;
      }
      std::vector<Realm::IndexSpace<DIM,T> > subspaces;
      Realm::ProfilingRequestSet requests;
      if (context->runtime->profiler != NULL)
        context->runtime->profiler->add_partition_request(requests,
                                              op, DEP_PART_UNIONS);
      if (op->has_execution_fence_event())
        preconditions.insert(op->get_execution_fence_event());
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      ApEvent result(Realm::IndexSpace<DIM,T>::compute_unions(
            lhs_spaces, rhs_spaces, subspaces, requests, precondition));
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists() || (result == precondition))
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      LegionSpy::log_deppart_events(op->get_unique_op_id(),
                                    handle, precondition, result);
#endif
      // Now set the index spaces for the results
      subspace_index = 0;
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          IndexSpaceNodeT<DIM,T> *child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[subspace_index++]))
            assert(false); // should never hit this
        }
      }
      else
      {
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          IndexSpaceNodeT<DIM,T> *child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[subspace_index++]))
            assert(false); // should never hit this
        }
        delete itr;
      }
      return result;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_by_intersection(Operation *op,
                                                      IndexPartNode *partition,
                                                      IndexPartNode *left,
                                                      IndexPartNode *right)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(partition->parent == this);
#endif
      const size_t count = partition->color_space->get_volume();
      std::vector<Realm::IndexSpace<DIM,T> > lhs_spaces(count);
      std::vector<Realm::IndexSpace<DIM,T> > rhs_spaces(count);
      std::set<ApEvent> preconditions;
      // First we need to fill in all the subspaces
      unsigned subspace_index = 0;
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          IndexSpaceNodeT<DIM,T> *left_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(left->get_child(color));
          IndexSpaceNodeT<DIM,T> *right_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(right->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < count);
#endif
          ApEvent left_ready = 
            left_child->get_realm_index_space(lhs_spaces[subspace_index],
                                              false/*tight*/);
          ApEvent right_ready = 
            right_child->get_realm_index_space(rhs_spaces[subspace_index++],
                                               false/*tight*/);
          if (left_ready.exists())
            preconditions.insert(left_ready);
          if (right_ready.exists())
            preconditions.insert(right_ready);
        }
      }
      else
      {
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          IndexSpaceNodeT<DIM,T> *left_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
          IndexSpaceNodeT<DIM,T> *right_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(right->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < count);
#endif
          ApEvent left_ready = 
            left_child->get_realm_index_space(lhs_spaces[subspace_index],
                                              false/*tight*/);
          ApEvent right_ready = 
            right_child->get_realm_index_space(rhs_spaces[subspace_index++],
                                               false/*tight*/);
          if (left_ready.exists())
            preconditions.insert(left_ready);
          if (right_ready.exists())
            preconditions.insert(right_ready);
        }
        delete itr;
      }
      std::vector<Realm::IndexSpace<DIM,T> > subspaces;
      Realm::ProfilingRequestSet requests;
      if (context->runtime->profiler != NULL)
        context->runtime->profiler->add_partition_request(requests,
                                        op, DEP_PART_INTERSECTIONS);
      if (op->has_execution_fence_event())
        preconditions.insert(op->get_execution_fence_event());
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      ApEvent result(Realm::IndexSpace<DIM,T>::compute_intersections(
            lhs_spaces, rhs_spaces, subspaces, requests, precondition));
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists() || (result == precondition))
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      LegionSpy::log_deppart_events(op->get_unique_op_id(),
                                    handle, precondition, result);
#endif
      // Now set the index spaces for the results
      subspace_index = 0;
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          IndexSpaceNodeT<DIM,T> *child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[subspace_index++]))
            assert(false); // should never hit this
        }
      }
      else
      {
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          IndexSpaceNodeT<DIM,T> *child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[subspace_index++]))
            assert(false); // should never hit this
        }
        delete itr;
      }
      return result;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_by_intersection(Operation *op,
                                                      IndexPartNode *partition,
                                                      // Left is implicit "this"
                                                      IndexPartNode *right,
                                                      const bool dominates)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(partition->parent == this);
#endif
      const size_t count = partition->color_space->get_volume();
      std::vector<Realm::IndexSpace<DIM,T> > rhs_spaces(count);
      std::set<ApEvent> preconditions;
      // First we need to fill in all the subspaces
      unsigned subspace_index = 0;
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          IndexSpaceNodeT<DIM,T> *right_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(right->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < count);
#endif
          ApEvent right_ready = 
            right_child->get_realm_index_space(rhs_spaces[subspace_index++],
                                               false/*tight*/);
          if (right_ready.exists())
            preconditions.insert(right_ready);
        }
      }
      else
      {
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          IndexSpaceNodeT<DIM,T> *right_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(right->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < count);
#endif
          ApEvent right_ready = 
            right_child->get_realm_index_space(rhs_spaces[subspace_index++],
                                               false/*tight*/);
          if (right_ready.exists())
            preconditions.insert(right_ready);
        }
        delete itr;
      }
      ApEvent result, precondition;
      std::vector<Realm::IndexSpace<DIM,T> > subspaces;
      if (dominates)
      {
        // If we've been told that we dominate then there is no
        // need to event do the intersection tests at all
        subspaces.swap(rhs_spaces);
        result = Runtime::merge_events(NULL, preconditions);
      }
      else
      {
        Realm::ProfilingRequestSet requests;
        if (context->runtime->profiler != NULL)
          context->runtime->profiler->add_partition_request(requests,
                                          op, DEP_PART_INTERSECTIONS);
        Realm::IndexSpace<DIM,T> lhs_space;
        ApEvent left_ready = get_realm_index_space(lhs_space, false/*tight*/);
        if (left_ready.exists())
          preconditions.insert(left_ready);
        if (op->has_execution_fence_event())
          preconditions.insert(op->get_execution_fence_event());
        precondition = Runtime::merge_events(NULL, preconditions);
        result = ApEvent(Realm::IndexSpace<DIM,T>::compute_intersections(
              lhs_space, rhs_spaces, subspaces, requests, precondition));  
      }
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists() || (result == precondition))
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      LegionSpy::log_deppart_events(op->get_unique_op_id(),
                                    handle, precondition, result);
#endif
      // Now set the index spaces for the results
      subspace_index = 0;
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          IndexSpaceNodeT<DIM,T> *child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[subspace_index++]))
            assert(false); // should never hit this
        }
      }
      else
      {
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          IndexSpaceNodeT<DIM,T> *child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[subspace_index++]))
            assert(false); // should never hit this
        }
        delete itr;
      }
      return result;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_by_difference(Operation *op,
                                                      IndexPartNode *partition,
                                                      IndexPartNode *left,
                                                      IndexPartNode *right)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(partition->parent == this);
#endif
      const size_t count = partition->color_space->get_volume();
      std::vector<Realm::IndexSpace<DIM,T> > lhs_spaces(count);
      std::vector<Realm::IndexSpace<DIM,T> > rhs_spaces(count);
      std::set<ApEvent> preconditions;
      // First we need to fill in all the subspaces
      unsigned subspace_index = 0;
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          IndexSpaceNodeT<DIM,T> *left_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(left->get_child(color));
          IndexSpaceNodeT<DIM,T> *right_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(right->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < count);
#endif
          ApEvent left_ready = 
            left_child->get_realm_index_space(lhs_spaces[subspace_index],
                                              false/*tight*/);
          ApEvent right_ready = 
            right_child->get_realm_index_space(rhs_spaces[subspace_index++],
                                               false/*tight*/);
          if (left_ready.exists())
            preconditions.insert(left_ready);
          if (right_ready.exists())
            preconditions.insert(right_ready);
        }
      }
      else
      {
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          IndexSpaceNodeT<DIM,T> *left_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
          IndexSpaceNodeT<DIM,T> *right_child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(right->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < count);
#endif
          ApEvent left_ready = 
            left_child->get_realm_index_space(lhs_spaces[subspace_index],
                                              false/*tight*/);
          ApEvent right_ready = 
            right_child->get_realm_index_space(rhs_spaces[subspace_index++],
                                               false/*tight*/);
          if (left_ready.exists())
            preconditions.insert(left_ready);
          if (right_ready.exists())
            preconditions.insert(right_ready);
        }
        delete itr;
      }
      std::vector<Realm::IndexSpace<DIM,T> > subspaces;
      Realm::ProfilingRequestSet requests;
      if (context->runtime->profiler != NULL)
        context->runtime->profiler->add_partition_request(requests,
                                          op, DEP_PART_DIFFERENCES);
      if (op->has_execution_fence_event())
        preconditions.insert(op->get_execution_fence_event());
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      ApEvent result(Realm::IndexSpace<DIM,T>::compute_differences(
            lhs_spaces, rhs_spaces, subspaces, requests, precondition));
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists() || (result == precondition))
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      LegionSpy::log_deppart_events(op->get_unique_op_id(),
                                    handle, precondition, result);
#endif
      // Now set the index spaces for the results
      subspace_index = 0;
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          IndexSpaceNodeT<DIM,T> *child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[subspace_index++]))
            assert(false); // should never hit this
        }
      }
      else
      {
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          IndexSpaceNodeT<DIM,T> *child = 
            static_cast<IndexSpaceNodeT<DIM,T>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(subspace_index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[subspace_index++]))
            assert(false); // should never hit this
        }
        delete itr;
      }
      return result;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_by_restriction(
                                                      IndexPartNode *partition,
                                                      const void *tran,
                                                      const void *ext,
                                                      int partition_dim)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      // should be called on the color space
      assert(this == partition->color_space); 
#endif
      switch (partition_dim)
      {
#define DIMFUNC(D2) \
        case D2: \
          { \
            const Realm::Matrix<D2,DIM,T> *transform = \
              static_cast<const Realm::Matrix<D2,DIM,T>*>(tran); \
            const Realm::Rect<D2,T> *extent = \
              static_cast<const Realm::Rect<D2,T>*>(ext); \
            return create_by_restriction_helper<D2>(partition, \
                                                   *transform, *extent); \
          }
        LEGION_FOREACH_N(DIMFUNC)
#undef DIMFUNC
        default:
          assert(false);
      }
      return ApEvent::NO_AP_EVENT;
    }

    //--------------------------------------------------------------------------
    template<int N, typename T> template<int M>
    ApEvent IndexSpaceNodeT<N,T>::create_by_restriction_helper(
                                        IndexPartNode *partition,
                                        const Realm::Matrix<M,N,T> &transform,
                                        const Realm::Rect<M,T> &extent)
    //--------------------------------------------------------------------------
    {
      // Get the parent index space in case it has a sparsity map
      IndexSpaceNodeT<M,T> *parent = 
                      static_cast<IndexSpaceNodeT<M,T>*>(partition->parent);
      // No need to wait since we'll just be messing with the bounds
      Realm::IndexSpace<M,T> parent_is;
      parent->get_realm_index_space(parent_is, true/*tight*/);
      Realm::IndexSpace<N,T> local_is;
      get_realm_index_space(local_is, true/*tight*/);
      // Iterate over our points (colors) and fill in the bounds
      for (Realm::IndexSpaceIterator<N,T> rect_itr(local_is); 
            rect_itr.valid; rect_itr.step())
      {
        for (Realm::PointInRectIterator<N,T> color_itr(rect_itr.rect); 
              color_itr.valid; color_itr.step())
        {
          // Copy the index space from the parent
          Realm::IndexSpace<M,T> child_is = parent_is;
          // Compute the new bounds and intersect it with the parent bounds
          child_is.bounds = parent_is.bounds.intersection(
                              extent + transform * color_itr.p);
          // Get the legion color
          LegionColor color = linearize_color(&color_itr.p, 
                                              handle.get_type_tag());
          // Get the appropriate child
          IndexSpaceNodeT<M,T> *child = 
            static_cast<IndexSpaceNodeT<M,T>*>(partition->get_child(color));
          // Then set the new index space
          if (child->set_realm_index_space(context->runtime->address_space, 
                                           child_is))
            assert(false); // should never hit this
        }
      }
      // Our only precondition is that the parent index space is computed
      return parent->index_space_ready;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_by_field(Operation *op,
                                                    IndexPartNode *partition,
                              const std::vector<FieldDataDescriptor> &instances,
                                                    ApEvent instances_ready)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(partition->parent == this);
#endif
      // Demux the color space type to do the actual operations 
      CreateByFieldHelper creator(this, op, partition, 
                                  instances, instances_ready);
      NT_TemplateHelper::demux<CreateByFieldHelper>(
                   partition->color_space->handle.get_type_tag(), &creator);
      return creator.result;
    }
#endif // defined(DEFINE_NT_TEMPLATES)

#ifdef DEFINE_NTNT_TEMPLATES
    //--------------------------------------------------------------------------
    template<int DIM, typename T> template<int COLOR_DIM, typename COLOR_T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_by_field_helper(Operation *op,
                                                      IndexPartNode *partition,
                             const std::vector<FieldDataDescriptor> &instances,
                                                       ApEvent instances_ready)
    //--------------------------------------------------------------------------
    {
      IndexSpaceNodeT<COLOR_DIM,COLOR_T> *color_space = 
       static_cast<IndexSpaceNodeT<COLOR_DIM,COLOR_T>*>(partition->color_space);
      // Enumerate the color space
      Realm::IndexSpace<COLOR_DIM,COLOR_T> realm_color_space;
      color_space->get_realm_index_space(realm_color_space, true/*tight*/);
      const size_t num_colors = realm_color_space.volume();
      std::vector<Realm::Point<COLOR_DIM,COLOR_T> > colors(num_colors);
      unsigned index = 0;
      for (Realm::IndexSpaceIterator<COLOR_DIM,COLOR_T> 
            rect_iter(realm_color_space); rect_iter.valid; rect_iter.step())
      {
        for (Realm::PointInRectIterator<COLOR_DIM,COLOR_T> 
              itr(rect_iter.rect); itr.valid; itr.step())
        {
#ifdef DEBUG_LEGION
          assert(index < colors.size());
#endif
          colors[index++] = itr.p;
        }
      }
      // Translate the instances to realm field data descriptors
      typedef Realm::FieldDataDescriptor<Realm::IndexSpace<DIM,T>,
                Realm::Point<COLOR_DIM,COLOR_T> > RealmDescriptor;
      std::vector<RealmDescriptor> descriptors(instances.size());
      std::set<ApEvent> preconditions; 
      for (unsigned idx = 0; idx < instances.size(); idx++)
      {
        const FieldDataDescriptor &src = instances[idx];
        RealmDescriptor &dst = descriptors[idx];
        dst.inst = src.inst;
        dst.field_offset = src.field_offset;
        IndexSpaceNodeT<DIM,T> *node = static_cast<IndexSpaceNodeT<DIM,T>*>(
                                          context->get_node(src.index_space));
        ApEvent ready = node->get_realm_index_space(dst.index_space, 
                                                    false/*tight*/);
        if (ready.exists())
          preconditions.insert(ready);
      }
      // Get the profiling requests
      Realm::ProfilingRequestSet requests;
      if (context->runtime->profiler != NULL)
        context->runtime->profiler->add_partition_request(requests,
                                            op, DEP_PART_BY_FIELD);
      // Perform the operation
      std::vector<Realm::IndexSpace<DIM,T> > subspaces;
      Realm::IndexSpace<DIM,T> local_space;
      ApEvent ready = get_realm_index_space(local_space, false/*tight*/);
      if (ready.exists())
        preconditions.insert(ready);
      preconditions.insert(instances_ready);
      if (op->has_execution_fence_event())
        preconditions.insert(op->get_execution_fence_event());
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      ApEvent result(local_space.create_subspaces_by_field(
            descriptors, colors, subspaces, requests, precondition));
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists() || (result == precondition))
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      LegionSpy::log_deppart_events(op->get_unique_op_id(),handle,
                                    precondition, result);
#endif
      // Update the children with the names of their subspaces 
      for (unsigned idx = 0; idx < colors.size(); idx++)
      {
        LegionColor child_color = color_space->linearize_color(&colors[idx],
                                        color_space->handle.get_type_tag());
        IndexSpaceNodeT<DIM,T> *child = static_cast<IndexSpaceNodeT<DIM,T>*>(
                                            partition->get_child(child_color));
        if (child->set_realm_index_space(context->runtime->address_space,
                                         subspaces[idx]))
          assert(false); // should never hit this
      }
      return result;
    }
#endif // defined(DEFINE_NTNT_TEMPLATES)

#ifdef DEFINE_NT_TEMPLATES
    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_by_image(Operation *op,
                                                    IndexPartNode *partition,
                                                    IndexPartNode *projection,
                            const std::vector<FieldDataDescriptor> &instances,
                                                    ApEvent instances_ready)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(partition->parent == this);
#endif
      // Demux the projection type to do the actual operations
      CreateByImageHelper creator(this, op, partition, projection,
                                  instances, instances_ready);
      NT_TemplateHelper::demux<CreateByImageHelper>(
          projection->handle.get_type_tag(), &creator);
      return creator.result;
    }
#endif // defined(DEFINE_NT_TEMPLATES)

#ifdef DEFINE_NTNT_TEMPLATES    
    //--------------------------------------------------------------------------
    template<int DIM1, typename T1> template<int DIM2, typename T2>
    ApEvent IndexSpaceNodeT<DIM1,T1>::create_by_image_helper(Operation *op,
                                                    IndexPartNode *partition,
                                                    IndexPartNode *projection,
                            const std::vector<FieldDataDescriptor> &instances,
                                                    ApEvent instances_ready)
    //--------------------------------------------------------------------------
    {
      // Get the index spaces of the projection partition
      std::vector<Realm::IndexSpace<DIM2,T2> > 
                                sources(projection->color_space->get_volume());
      std::set<ApEvent> preconditions; 
      if (partition->total_children == partition->max_linearized_color)
      {
        // Always use the partitions color space
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM2,T2> *child = 
           static_cast<IndexSpaceNodeT<DIM2,T2>*>(projection->get_child(color));
          ApEvent ready = child->get_realm_index_space(sources[color],
                                                       false/*tight*/);
          if (ready.exists())
            preconditions.insert(ready);
        }
      }
      else
      {
        unsigned index = 0;
        // Always use the partitions color space
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM2,T2> *child = 
           static_cast<IndexSpaceNodeT<DIM2,T2>*>(projection->get_child(color));
#ifdef DEBUG_LEGION
          assert(index < sources.size());
#endif
          ApEvent ready = child->get_realm_index_space(sources[index++],
                                                       false/*tight*/);
          if (ready.exists())
            preconditions.insert(ready);
        }
        delete itr;
      }
      // Translate the descriptors into realm descriptors
      typedef Realm::FieldDataDescriptor<Realm::IndexSpace<DIM2,T2>,
                                       Realm::Point<DIM1,T1> > RealmDescriptor;
      std::vector<RealmDescriptor> descriptors(instances.size());
      for (unsigned idx = 0; idx < instances.size(); idx++)
      {
        const FieldDataDescriptor &src = instances[idx];
        RealmDescriptor &dst = descriptors[idx];
        dst.inst = src.inst;
        dst.field_offset = src.field_offset;
        IndexSpaceNodeT<DIM2,T2> *node = static_cast<IndexSpaceNodeT<DIM2,T2>*>(
                                          context->get_node(src.index_space));
        ApEvent ready = node->get_realm_index_space(dst.index_space,
                                                    false/*tight*/);
        if (ready.exists())
          preconditions.insert(ready);
      }
      // Get the profiling requests
      Realm::ProfilingRequestSet requests;
      if (context->runtime->profiler != NULL)
        context->runtime->profiler->add_partition_request(requests,
                                            op, DEP_PART_BY_IMAGE);
      // Perform the operation
      std::vector<Realm::IndexSpace<DIM1,T1> > subspaces;
      Realm::IndexSpace<DIM1,T1> local_space;
      ApEvent ready = get_realm_index_space(local_space, false/*tight*/);
      if (ready.exists())
        preconditions.insert(ready);
      preconditions.insert(instances_ready);
      if (op->has_execution_fence_event())
        preconditions.insert(op->get_execution_fence_event());
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      ApEvent result(local_space.create_subspaces_by_image(descriptors,
            sources, subspaces, requests, precondition));
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists() || (result == precondition))
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      LegionSpy::log_deppart_events(op->get_unique_op_id(),handle,
                                    precondition, result);
#endif
      // Update the child subspaces of the image
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM1,T1> *child = 
           static_cast<IndexSpaceNodeT<DIM1,T1>*>(partition->get_child(color));
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[color]))
            assert(false); // should never hit this
        }
      }
      else
      {
        unsigned index = 0;
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM1,T1> *child = 
           static_cast<IndexSpaceNodeT<DIM1,T1>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[index++]))
            assert(false); // should never hit this
        }
        delete itr;
      }
      return result;
    }
#endif // defined(DEFINE_NTNT_TEMPLATES)

#ifdef DEFINE_NT_TEMPLATES
    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_by_image_range(Operation *op,
                                                    IndexPartNode *partition,
                                                    IndexPartNode *projection,
                            const std::vector<FieldDataDescriptor> &instances,
                                                    ApEvent instances_ready)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(partition->parent == this);
#endif
      // Demux the projection type to do the actual operations
      CreateByImageRangeHelper creator(this, op, partition, projection,
                                       instances, instances_ready);
      NT_TemplateHelper::demux<CreateByImageRangeHelper>(
          projection->handle.get_type_tag(), &creator);
      return creator.result;
    }
#endif // defined(DEFINE_NT_TEMPLATES)

#ifdef DEFINE_NTNT_TEMPLATES
    //--------------------------------------------------------------------------
    template<int DIM1, typename T1> template<int DIM2, typename T2>
    ApEvent IndexSpaceNodeT<DIM1,T1>::create_by_image_range_helper(
                                                    Operation *op,
                                                    IndexPartNode *partition,
                                                    IndexPartNode *projection,
                            const std::vector<FieldDataDescriptor> &instances,
                                                    ApEvent instances_ready)
    //--------------------------------------------------------------------------
    {
      // Get the index spaces of the projection partition
      std::vector<Realm::IndexSpace<DIM2,T2> > 
                                sources(projection->color_space->get_volume());
      std::set<ApEvent> preconditions;
      if (partition->total_children == partition->max_linearized_color)
      {
        // Always use the partitions color space
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM2,T2> *child = 
           static_cast<IndexSpaceNodeT<DIM2,T2>*>(projection->get_child(color));
          ApEvent ready = child->get_realm_index_space(sources[color],
                                                       false/*tight*/);
          if (ready.exists())
            preconditions.insert(ready);
        }
      }
      else
      {
        unsigned index = 0;
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        // Always use the partitions color space
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM2,T2> *child = 
           static_cast<IndexSpaceNodeT<DIM2,T2>*>(projection->get_child(color));
#ifdef DEBUG_LEGION
          assert(index < sources.size());
#endif
          ApEvent ready = child->get_realm_index_space(sources[index++],
                                                       false/*tight*/);
          if (ready.exists())
            preconditions.insert(ready);
        }
        delete itr;
      }
      // Translate the descriptors into realm descriptors
      typedef Realm::FieldDataDescriptor<Realm::IndexSpace<DIM2,T2>,
                                       Realm::Rect<DIM1,T1> > RealmDescriptor;
      std::vector<RealmDescriptor> descriptors(instances.size());
      for (unsigned idx = 0; idx < instances.size(); idx++)
      {
        const FieldDataDescriptor &src = instances[idx];
        RealmDescriptor &dst = descriptors[idx];
        dst.inst = src.inst;
        dst.field_offset = src.field_offset;
        IndexSpaceNodeT<DIM2,T2> *node = static_cast<IndexSpaceNodeT<DIM2,T2>*>(
                                          context->get_node(src.index_space));
        ApEvent ready = node->get_realm_index_space(dst.index_space,
                                                    false/*tight*/);
        if (ready.exists())
          preconditions.insert(ready);
      }
      // Get the profiling requests
      Realm::ProfilingRequestSet requests;
      if (context->runtime->profiler != NULL)
        context->runtime->profiler->add_partition_request(requests,
                                            op, DEP_PART_BY_IMAGE_RANGE);
      // Perform the operation
      std::vector<Realm::IndexSpace<DIM1,T1> > subspaces;
      Realm::IndexSpace<DIM1,T1> local_space;
      ApEvent ready = get_realm_index_space(local_space, false/*tight*/);
      if (ready.exists())
        preconditions.insert(ready);
      preconditions.insert(instances_ready);
      if (op->has_execution_fence_event())
        preconditions.insert(op->get_execution_fence_event());
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      ApEvent result(local_space.create_subspaces_by_image(descriptors,
            sources, subspaces, requests, precondition));
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists() || (result == precondition))
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      LegionSpy::log_deppart_events(op->get_unique_op_id(),handle,
                                    precondition, result);
#endif
      // Update the child subspaces of the image
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM1,T1> *child = 
           static_cast<IndexSpaceNodeT<DIM1,T1>*>(partition->get_child(color));
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[color]))
            assert(false); // should never hit this
        }
      }
      else
      {
        unsigned index = 0;
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM1,T1> *child = 
           static_cast<IndexSpaceNodeT<DIM1,T1>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[index++]))
            assert(false); // should never hit this
        }
        delete itr;
      }
      return result;
    }
#endif // defined(DEFINE_NTNT_TEMPLATES)

#ifdef DEFINE_NT_TEMPLATES
    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_by_preimage(Operation *op,
                                                    IndexPartNode *partition,
                                                    IndexPartNode *projection,
                            const std::vector<FieldDataDescriptor> &instances,
                                                    ApEvent instances_ready)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(partition->parent == this);
#endif
      // Demux the projection type to do the actual operations
      CreateByPreimageHelper creator(this, op, partition, projection,
                                     instances, instances_ready);
      NT_TemplateHelper::demux<CreateByPreimageHelper>(
          projection->handle.get_type_tag(), &creator);
      return creator.result;
    }
#endif // defined(DEFINE_NT_TEMPLATES)

#ifdef DEFINE_NTNT_TEMPLATES
    //--------------------------------------------------------------------------
    template<int DIM1, typename T1> template<int DIM2, typename T2>
    ApEvent IndexSpaceNodeT<DIM1,T1>::create_by_preimage_helper(Operation *op,
                                                    IndexPartNode *partition,
                                                    IndexPartNode *projection,
                            const std::vector<FieldDataDescriptor> &instances,
                                                    ApEvent instances_ready)
    //--------------------------------------------------------------------------
    {
      // Get the index spaces of the projection partition
      std::vector<Realm::IndexSpace<DIM2,T2> > 
                                targets(projection->color_space->get_volume());
      std::set<ApEvent> preconditions;
      if (partition->total_children == partition->max_linearized_color)
      {
        // Always use the partitions color space
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM2,T2> *child = 
           static_cast<IndexSpaceNodeT<DIM2,T2>*>(projection->get_child(color));
          ApEvent ready = child->get_realm_index_space(targets[color],
                                                       false/*tight*/);
          if (ready.exists())
            preconditions.insert(ready);
        }
      }
      else
      {
        unsigned index = 0;
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        // Always use the partitions color space
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM2,T2> *child = 
           static_cast<IndexSpaceNodeT<DIM2,T2>*>(projection->get_child(color));
#ifdef DEBUG_LEGION
          assert(index < targets.size());
#endif
          ApEvent ready = child->get_realm_index_space(targets[index++],
                                                       false/*tight*/);
          if (ready.exists())
            preconditions.insert(ready);
        }
        delete itr;
      }
      // Translate the descriptors into realm descriptors
      typedef Realm::FieldDataDescriptor<Realm::IndexSpace<DIM1,T1>,
                                       Realm::Point<DIM2,T2> > RealmDescriptor;
      std::vector<RealmDescriptor> descriptors(instances.size());
      for (unsigned idx = 0; idx < instances.size(); idx++)
      {
        const FieldDataDescriptor &src = instances[idx];
        RealmDescriptor &dst = descriptors[idx];
        dst.inst = src.inst;
        dst.field_offset = src.field_offset;
        IndexSpaceNodeT<DIM1,T1> *node = static_cast<IndexSpaceNodeT<DIM1,T1>*>(
                                          context->get_node(src.index_space));
        ApEvent ready = node->get_realm_index_space(dst.index_space,
                                                    false/*tight*/);
        if (ready.exists())
          preconditions.insert(ready);
      }
      // Get the profiling requests
      Realm::ProfilingRequestSet requests;
      if (context->runtime->profiler != NULL)
        context->runtime->profiler->add_partition_request(requests,
                                            op, DEP_PART_BY_PREIMAGE);
      // Perform the operation
      std::vector<Realm::IndexSpace<DIM1,T1> > subspaces;
      Realm::IndexSpace<DIM1,T1> local_space;
      ApEvent ready = get_realm_index_space(local_space, false/*tight*/);
      if (ready.exists())
        preconditions.insert(ready);
      preconditions.insert(instances_ready);
      if (op->has_execution_fence_event())
        preconditions.insert(op->get_execution_fence_event());
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      ApEvent result(local_space.create_subspaces_by_preimage(
            descriptors, targets, subspaces, requests, precondition));
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists() || (result == precondition))
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      LegionSpy::log_deppart_events(op->get_unique_op_id(),handle,
                                    precondition, result);
#endif
      // Update the child subspace of the preimage
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM1,T1> *child = 
           static_cast<IndexSpaceNodeT<DIM1,T1>*>(partition->get_child(color));
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[color]))
            assert(false); // should never hit this
        }
      }
      else
      {
        unsigned index = 0;
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM1,T1> *child = 
           static_cast<IndexSpaceNodeT<DIM1,T1>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[index++]))
            assert(false); // should never hit this
        }
        delete itr;
      }
      return result;
    }
#endif // defined(DEFINE_NTNT_TEMPLATES)

#ifdef DEFINE_NT_TEMPLATES
    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_by_preimage_range(Operation *op,
                                                    IndexPartNode *partition,
                                                    IndexPartNode *projection,
                            const std::vector<FieldDataDescriptor> &instances,
                                                    ApEvent instances_ready)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(partition->parent == this);
#endif
      // Demux the projection type to do the actual operations
      CreateByPreimageRangeHelper creator(this, op, partition, projection,
                                          instances, instances_ready);
      NT_TemplateHelper::demux<CreateByPreimageRangeHelper>(
          projection->handle.get_type_tag(), &creator);
      return creator.result;
    }
#endif // defined(DEFINE_NT_TEMPLATES)

#ifdef DEFINE_NTNT_TEMPLATES
    //--------------------------------------------------------------------------
    template<int DIM1, typename T1> template<int DIM2, typename T2>
    ApEvent IndexSpaceNodeT<DIM1,T1>::create_by_preimage_range_helper(
                                                    Operation *op,
                                                    IndexPartNode *partition,
                                                    IndexPartNode *projection,
                            const std::vector<FieldDataDescriptor> &instances,
                                                    ApEvent instances_ready)
    //--------------------------------------------------------------------------
    {
      // Get the index spaces of the projection partition
      std::vector<Realm::IndexSpace<DIM2,T2> > 
                                targets(projection->color_space->get_volume());
      std::set<ApEvent> preconditions;
      if (partition->total_children == partition->max_linearized_color)
      {
        // Always use the partitions color space
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM2,T2> *child = 
           static_cast<IndexSpaceNodeT<DIM2,T2>*>(projection->get_child(color));
          ApEvent ready = child->get_realm_index_space(targets[color],
                                                       false/*tight*/);
          if (ready.exists())
            preconditions.insert(ready);
        }
      }
      else
      {
        unsigned index = 0;
        // Always use the partitions color space
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM2,T2> *child = 
           static_cast<IndexSpaceNodeT<DIM2,T2>*>(projection->get_child(color));
#ifdef DEBUG_LEGION
          assert(index < targets.size());
#endif
          ApEvent ready = child->get_realm_index_space(targets[index++],
                                                       false/*tight*/);
          if (ready.exists())
            preconditions.insert(ready);
        }
        delete itr;
      }
      // Translate the descriptors into realm descriptors
      typedef Realm::FieldDataDescriptor<Realm::IndexSpace<DIM1,T1>,
                                       Realm::Rect<DIM2,T2> > RealmDescriptor;
      std::vector<RealmDescriptor> descriptors(instances.size());
      for (unsigned idx = 0; idx < instances.size(); idx++)
      {
        const FieldDataDescriptor &src = instances[idx];
        RealmDescriptor &dst = descriptors[idx];
        dst.inst = src.inst;
        dst.field_offset = src.field_offset;
        IndexSpaceNodeT<DIM1,T1> *node = static_cast<IndexSpaceNodeT<DIM1,T1>*>(
                                          context->get_node(src.index_space));
        ApEvent ready = node->get_realm_index_space(dst.index_space,
                                                    false/*tight*/);
        if (ready.exists())
          preconditions.insert(ready);
      }
      // Get the profiling requests
      Realm::ProfilingRequestSet requests;
      if (context->runtime->profiler != NULL)
        context->runtime->profiler->add_partition_request(requests,
                                            op, DEP_PART_BY_PREIMAGE_RANGE);
      // Perform the operation
      std::vector<Realm::IndexSpace<DIM1,T1> > subspaces;
      Realm::IndexSpace<DIM1,T1> local_space;
      ApEvent ready = get_realm_index_space(local_space, false/*tight*/);
      if (ready.exists())
        preconditions.insert(ready);
      preconditions.insert(instances_ready);
      if (op->has_execution_fence_event())
        preconditions.insert(op->get_execution_fence_event());
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      ApEvent result(local_space.create_subspaces_by_preimage(
            descriptors, targets, subspaces, requests, precondition));
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists() || (result == precondition))
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      LegionSpy::log_deppart_events(op->get_unique_op_id(),handle,
                                    precondition, result);
#endif
      // Update the child subspace of the preimage
      if (partition->total_children == partition->max_linearized_color)
      {
        for (LegionColor color = 0; color < partition->total_children; color++)
        {
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM1,T1> *child = 
           static_cast<IndexSpaceNodeT<DIM1,T1>*>(partition->get_child(color));
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[color]))
            assert(false); // should never hit this
        }
      }
      else
      {
        unsigned index = 0;
        ColorSpaceIterator *itr = 
          partition->color_space->create_color_space_iterator();
        while (itr->is_valid())
        {
          const LegionColor color = itr->yield_color();
          // Get the child of the projection partition
          IndexSpaceNodeT<DIM1,T1> *child = 
           static_cast<IndexSpaceNodeT<DIM1,T1>*>(partition->get_child(color));
#ifdef DEBUG_LEGION
          assert(index < subspaces.size());
#endif
          if (child->set_realm_index_space(context->runtime->address_space,
                                           subspaces[index++]))
            assert(false); // should never hit this
        }
        delete itr;
      }
      return result;
    }
#endif // defined(DEFINE_NTNT_TEMPLATES)

#ifdef DEFINE_NT_TEMPLATES
    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::create_association(Operation *op,
                                                       IndexSpaceNode *range,
                              const std::vector<FieldDataDescriptor> &instances,
                                                       ApEvent instances_ready)
    //--------------------------------------------------------------------------
    {
      // Demux the range type to do the actual operation
      CreateAssociationHelper creator(this, op, range, 
                                      instances, instances_ready);
      NT_TemplateHelper::demux<CreateAssociationHelper>(
          range->handle.get_type_tag(), &creator);
      return creator.result;
    }
#endif // defined(DEFINE_NT_TEMPLATES)

#ifdef DEFINE_NTNT_TEMPLATES
    //--------------------------------------------------------------------------
    template<int DIM1, typename T1> template<int DIM2, typename T2>
    ApEvent IndexSpaceNodeT<DIM1,T1>::create_association_helper(Operation *op,
                                                      IndexSpaceNode *range,
                              const std::vector<FieldDataDescriptor> &instances,
                                                      ApEvent instances_ready)
    //--------------------------------------------------------------------------
    {
      // Translate the descriptors into realm descriptors
      typedef Realm::FieldDataDescriptor<Realm::IndexSpace<DIM1,T1>,
                                       Realm::Point<DIM2,T2> > RealmDescriptor;
      std::vector<RealmDescriptor> descriptors(instances.size());
      std::set<ApEvent> preconditions;
      for (unsigned idx = 0; idx < instances.size(); idx++)
      {
        const FieldDataDescriptor &src = instances[idx];
        RealmDescriptor &dst = descriptors[idx];
        dst.inst = src.inst;
        dst.field_offset = src.field_offset;
        IndexSpaceNodeT<DIM1,T1> *node = static_cast<IndexSpaceNodeT<DIM1,T1>*>(
                                          context->get_node(src.index_space));
        ApEvent ready = node->get_realm_index_space(dst.index_space,
                                                    false/*tight*/);
        if (ready.exists())
          preconditions.insert(ready);
      }
      // Get the range index space
      IndexSpaceNodeT<DIM2,T2> *range_node = 
        static_cast<IndexSpaceNodeT<DIM2,T2>*>(range);
      Realm::IndexSpace<DIM2,T2> range_space;
      ApEvent range_ready = range_node->get_realm_index_space(range_space,
                                                              false/*tight*/);
      if (range_ready.exists())
        preconditions.insert(range_ready);
      // Get the profiling requests
      Realm::ProfilingRequestSet requests;
      if (context->runtime->profiler != NULL)
        context->runtime->profiler->add_partition_request(requests,
                                          op, DEP_PART_ASSOCIATION);
      Realm::IndexSpace<DIM1,T1> local_space;
      ApEvent local_ready = get_realm_index_space(local_space, false/*tight*/);
      if (local_ready.exists())
        preconditions.insert(local_ready);
      preconditions.insert(instances_ready);
      if (op->has_execution_fence_event())
        preconditions.insert(op->get_execution_fence_event());
      // Issue the operation
      ApEvent precondition = Runtime::merge_events(NULL, preconditions);
      ApEvent result(local_space.create_association(descriptors,
            range_space, requests, precondition));
#ifdef LEGION_DISABLE_EVENT_PRUNING
      if (!result.exists() || (result == precondition))
      {
        ApUserEvent new_result = Runtime::create_ap_user_event();
        Runtime::trigger_event(new_result);
        result = new_result;
      }
#endif
#ifdef LEGION_SPY
      LegionSpy::log_deppart_events(op->get_unique_op_id(),handle,
                                    precondition, result);
#endif
      return result;
    }
#endif // defined(DEFINE_NTNT_TEMPLATES)

#ifdef DEFINE_NT_TEMPLATES
    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexSpaceNodeT<DIM,T>::check_field_size(size_t field_size, bool range)
    //--------------------------------------------------------------------------
    {
      if (range)
        return (sizeof(Realm::Rect<DIM,T>) == field_size);
      else
        return (sizeof(Realm::Point<DIM,T>) == field_size);
    } 

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    PhysicalInstance IndexSpaceNodeT<DIM,T>::create_file_instance(
                                         const char *file_name,
                                         const std::vector<Realm::FieldID> &field_ids,
                                         const std::vector<size_t> &field_sizes,
                                         legion_file_mode_t file_mode,
                                         ApEvent &ready_event)
    //--------------------------------------------------------------------------
    {
      DETAILED_PROFILER(context->runtime, REALM_CREATE_INSTANCE_CALL);
      // Have to wait for the index space to be ready if necessary
      Realm::IndexSpace<DIM,T> local_space;
      get_realm_index_space(local_space, true/*tight*/);
      // No profiling for these kinds of instances currently
      Realm::ProfilingRequestSet requests;
      PhysicalInstance result;
      ready_event = ApEvent(PhysicalInstance::create_file_instance(result, 
          file_name, local_space, field_ids, field_sizes, file_mode, requests));
      return result;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    PhysicalInstance IndexSpaceNodeT<DIM,T>::create_hdf5_instance(
                                    const char *file_name,
				    const std::vector<Realm::FieldID> &field_ids,
                                    const std::vector<size_t> &field_sizes,
                                    const std::vector<const char*> &field_files,
                                    bool read_only, ApEvent &ready_event)
    //--------------------------------------------------------------------------
    {
      DETAILED_PROFILER(context->runtime, REALM_CREATE_INSTANCE_CALL);
      // Have to wait for the index space to be ready if necessary
      Realm::IndexSpace<DIM,T> local_space;
      get_realm_index_space(local_space, true/*tight*/);
      // No profiling for these kinds of instances currently
      Realm::ProfilingRequestSet requests;
      PhysicalInstance result;
#ifdef USE_HDF
      std::vector<PhysicalInstance::HDF5FieldInfo<DIM,T> >
	field_infos(field_ids.size());
      for (size_t i = 0; i < field_ids.size(); i++)
      {
	field_infos[i].field_id = field_ids[i];
	field_infos[i].field_size = field_sizes[i];
	field_infos[i].dataset_name = field_files[i];
	for (int j = 0; j < DIM; j++)
	  field_infos[i].offset[j] = 0;
	// HDF5 is always C-style layout, so reverse dimensions by default
	//  to match Legion's default of Fortran layout
	// TODO: actually use layout constraints here!
	for (int j = 0; j < DIM; j++)
	  field_infos[i].dim_order[j] = DIM - 1 - j;
      }
      ready_event = ApEvent(PhysicalInstance::create_hdf5_instance(result, 
                            file_name, local_space, field_infos,
		            read_only, requests));
#else
      assert(0 && "no HDF5 support");
      result = PhysicalInstance::NO_INST;
#endif
      return result;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    PhysicalInstance IndexSpaceNodeT<DIM,T>::create_external_instance(
                                          Memory memory, uintptr_t base,
                                          Realm::InstanceLayoutGeneric *ilg,
                                          ApEvent &ready_event)
    //--------------------------------------------------------------------------
    {
      DETAILED_PROFILER(context->runtime, REALM_CREATE_INSTANCE_CALL);
      // Have to wait for the index space to be ready if necessary
      Realm::IndexSpace<DIM,T> local_space;
      get_realm_index_space(local_space, true/*tight*/);
      // No profiling for these kinds of instances currently
      Realm::ProfilingRequestSet requests;
      PhysicalInstance result;
      ready_event = ApEvent(PhysicalInstance::create_external(result,
                                        memory, base, ilg, requests));
      return result;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::issue_fill(
                                 const PhysicalTraceInfo &trace_info,
                                 const std::vector<CopySrcDstField> &dst_fields,
                                 const void *fill_value, size_t fill_size,
#ifdef LEGION_SPY
                                 UniqueID fill_uid,
                                 FieldSpace handle,
                                 RegionTreeID tree_id,
#endif
                                 ApEvent precondition, PredEvent pred_guard,
                                 const FieldMaskSet<FillView> *tracing_srcs,
                                 const FieldMaskSet<InstanceView> *tracing_dsts)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> local_space;
      ApEvent space_ready = get_realm_index_space(local_space, true/*tight*/);
      if (precondition.exists() && space_ready.exists())
        return issue_fill_internal(context, local_space, trace_info, 
                                   dst_fields, fill_value, fill_size,
#ifdef LEGION_SPY
                                   fill_uid, handle, tree_id,
#endif
            Runtime::merge_events(&trace_info, space_ready, precondition),
            pred_guard, tracing_srcs, tracing_dsts);
      else if (space_ready.exists())
        return issue_fill_internal(context, local_space, trace_info, 
                                   dst_fields, fill_value, fill_size,
#ifdef LEGION_SPY
                                   fill_uid, handle, tree_id,
#endif
                                   space_ready, pred_guard,
                                   tracing_srcs, tracing_dsts);
      else
        return issue_fill_internal(context, local_space, trace_info, 
                                   dst_fields, fill_value, fill_size,
#ifdef LEGION_SPY
                                   fill_uid, handle, tree_id,
#endif
                                   precondition, pred_guard,
                                   tracing_srcs, tracing_dsts);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::issue_copy(
                                 const PhysicalTraceInfo &trace_info,
                                 const std::vector<CopySrcDstField> &dst_fields,
                                 const std::vector<CopySrcDstField> &src_fields,
#ifdef LEGION_SPY
                                 FieldSpace handle,
                                 RegionTreeID src_tree_id,
                                 RegionTreeID dst_tree_id,
#endif
                                 ApEvent precondition, PredEvent pred_guard,
                                 ReductionOpID redop, bool reduction_fold,
                                 const FieldMaskSet<InstanceView> *tracing_srcs,
                                 const FieldMaskSet<InstanceView> *tracing_dsts)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> local_space;
      ApEvent space_ready = get_realm_index_space(local_space, true/*tight*/);
      if (precondition.exists() && space_ready.exists())
        return issue_copy_internal(context, local_space, trace_info, dst_fields,
            src_fields,
#ifdef LEGION_SPY
            handle, src_tree_id, dst_tree_id,
#endif
            Runtime::merge_events(&trace_info, space_ready, precondition),
            pred_guard, redop, reduction_fold, tracing_srcs, tracing_dsts);
      else if (space_ready.exists())
        return issue_copy_internal(context, local_space, trace_info, 
                dst_fields, src_fields, 
#ifdef LEGION_SPY
                handle, src_tree_id, dst_tree_id,
#endif
                space_ready, pred_guard, redop, reduction_fold,
                tracing_srcs, tracing_dsts);
      else
        return issue_copy_internal(context, local_space, trace_info, 
                dst_fields, src_fields, 
#ifdef LEGION_SPY
                handle, src_tree_id, dst_tree_id,
#endif
                precondition, pred_guard, redop, reduction_fold,
                tracing_srcs, tracing_dsts);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::construct_indirections(
                                     const std::vector<unsigned> &field_indexes,
                                     const FieldID indirect_field,
                                     const TypeTag indirect_type,
                                     const bool is_range,
                                     const PhysicalInstance indirect_instance,
                                     const LegionVector<
                                            IndirectRecord>::aligned &records,
                                     std::vector<void*> &indirections,
                                     std::vector<unsigned> &indirect_indexes)
    //--------------------------------------------------------------------------
    {
      construct_indirections_internal<DIM,T>(field_indexes, indirect_field,
                                 indirect_type, is_range, indirect_instance,
                                 records, indirections, indirect_indexes);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::destroy_indirections(
                                               std::vector<void*> &indirections)
    //--------------------------------------------------------------------------
    {
      destroy_indirections_internal<DIM,T>(indirections);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ApEvent IndexSpaceNodeT<DIM,T>::issue_indirect(
                                 const PhysicalTraceInfo &trace_info,
                                 const std::vector<CopySrcDstField> &dst_fields,
                                 const std::vector<CopySrcDstField> &src_fields,
                                 const std::vector<void*> &indirects,
                                 ApEvent precondition, PredEvent pred_guard)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> local_space;
      ApEvent space_ready = get_realm_index_space(local_space, true/*tight*/);
      if (space_ready.exists() && precondition.exists())
        return issue_indirect_internal(context, local_space, trace_info, 
            dst_fields, src_fields, indirects,
            Runtime::merge_events(&trace_info, precondition, space_ready),
            pred_guard);
      else if (space_ready.exists())
        return issue_indirect_internal(context, local_space, trace_info, 
                                       dst_fields, src_fields, indirects, 
                                       space_ready, pred_guard);
      else
        return issue_indirect_internal(context, local_space, trace_info, 
                                       dst_fields, src_fields, indirects,
                                       precondition, pred_guard);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    Realm::InstanceLayoutGeneric* IndexSpaceNodeT<DIM,T>::create_layout(
                                    const Realm::InstanceLayoutConstraints &ilc,
                                    const OrderingConstraint &constraint)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> local_is;
      ApEvent space_ready = get_realm_index_space(local_is, true/*tight*/);
      if (space_ready.exists())
        space_ready.wait();
      return create_layout_internal(local_is, ilc, constraint); 
    }
    
    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::get_launch_space_domain(Domain &launch_domain)
    //--------------------------------------------------------------------------
    {
      DomainT<DIM,T> local_space;
      get_realm_index_space(local_space, true/*tight*/);
      launch_domain = local_space;
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::validate_slicing(
                                  const std::vector<IndexSpace> &slice_spaces, 
                                  MultiTask *task, MapperManager *mapper)
    //--------------------------------------------------------------------------
    {
      std::vector<IndexSpaceNodeT<DIM,T>*> slice_nodes(slice_spaces.size());
      for (unsigned idx = 0; idx < slice_spaces.size(); idx++)
      {
#ifdef DEBUG_LEGION
        assert(slice_spaces[idx].get_type_tag() == handle.get_type_tag());
#endif
        slice_nodes[idx] = static_cast<IndexSpaceNodeT<DIM,T>*>(
                            context->get_node(slice_spaces[idx]));
      }
      // Iterate over the points and make sure that they exist in exactly
      // one slice space, no more, no less
      Realm::IndexSpace<DIM,T> local_space;
      get_realm_index_space(local_space, true/*tight*/);
      for (PointInDomainIterator<DIM,T> itr(local_space); itr(); itr++)
      {
        bool found = false;
        const Realm::Point<DIM,T> &point = *itr;
        for (unsigned idx = 0; idx < slice_nodes.size(); idx++)
        {
          if (!slice_nodes[idx]->contains_point(point))
            continue;
          if (found)
            REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                    "Invalid mapper output from invocation of 'slice_task' "
                    "on mapper %s. Mapper returned multilple slices that "
                    "contained the same point for task %s (ID %lld)",
                    mapper->get_mapper_name(), task->get_task_name(),
                    task->get_unique_id())
          else
            found = true;
        }
        if (!found)
          REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                    "Invalid mapper output from invocation of 'slice_task' "
                    "on mapper %s. Mapper returned no slices that "
                    "contained some point(s) for task %s (ID %lld)",
                    mapper->get_mapper_name(), task->get_task_name(),
                    task->get_unique_id())
      }
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    void IndexSpaceNodeT<DIM,T>::log_launch_space(UniqueID op_id)
    //--------------------------------------------------------------------------
    {
      Realm::IndexSpace<DIM,T> local_space;
      get_realm_index_space(local_space, true/*tight*/);
      for (Realm::IndexSpaceIterator<DIM,T> itr(local_space); 
            itr.valid; itr.step())
        LegionSpy::log_launch_index_space_rect<DIM>(op_id, 
                                                    Rect<DIM,T>(itr.rect));
    }

    /////////////////////////////////////////////////////////////
    // Templated Color Space Iterator
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    ColorSpaceIteratorT<DIM,T>::ColorSpaceIteratorT(const DomainT<DIM,T> &d,
                                                    IndexSpaceNodeT<DIM,T> *cs)
      : ColorSpaceIterator(), PointInDomainIterator<DIM,T>(d), color_space(cs)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool ColorSpaceIteratorT<DIM,T>::is_valid(void) const
    //--------------------------------------------------------------------------
    {
      return this->valid();
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    LegionColor ColorSpaceIteratorT<DIM,T>::yield_color(void)
    //--------------------------------------------------------------------------
    {
      const LegionColor result = 
        color_space->linearize_color(*(this->point_itr));
      this->step();
      return result;
    }

    /////////////////////////////////////////////////////////////
    // Templated Index Partition Node 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexPartNodeT<DIM,T>::IndexPartNodeT(RegionTreeForest *ctx, 
                                        IndexPartition p,
                                        IndexSpaceNode *par, IndexSpaceNode *cs,
                                        LegionColor c, bool disjoint, 
                                        int complete, DistributedID did,
                                        ApEvent part_ready, ApUserEvent pend)
      : IndexPartNode(ctx, p, par, cs, c, disjoint,complete,did,part_ready,pend)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexPartNodeT<DIM,T>::IndexPartNodeT(RegionTreeForest *ctx, 
                                        IndexPartition p,
                                        IndexSpaceNode *par, IndexSpaceNode *cs,
                                        LegionColor c, RtEvent disjoint_event,
                                        int complete, DistributedID did,
                                        ApEvent partition_ready, 
                                        ApUserEvent pending)
      : IndexPartNode(ctx, p, par, cs, c, disjoint_event, complete, did, 
                      partition_ready, pending)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexPartNodeT<DIM,T>::IndexPartNodeT(const IndexPartNodeT &rhs)
      : IndexPartNode(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexPartNodeT<DIM,T>::~IndexPartNodeT(void)
    //--------------------------------------------------------------------------
    { 
    }

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    IndexPartNodeT<DIM,T>& IndexPartNodeT<DIM,T>::operator=(
                                                      const IndexPartNodeT &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    } 

    //--------------------------------------------------------------------------
    template<int DIM, typename T>
    bool IndexPartNodeT<DIM,T>::destroy_node(AddressSpaceID source, bool top,
                                             std::set<RtEvent> &applied) 
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(registered_with_runtime);
#endif
      if (destroyed)
      {
        // Deletion operations for different parts of the index space tree
        // can actually race to get here, so we don't report any races here
#if 0
        if (top)
          REPORT_LEGION_ERROR(ERROR_ILLEGAL_INDEX_PARTITION_DELETION,
              "Duplicate deletion of Index Partition %d", handle.get_id())
        else
#endif
        return false;
      }
      destroyed = true;
      // If we're not the owner send a message to do the destruction
      // otherwise we can do it here
      if (!is_owner())
      {
        runtime->send_index_partition_destruction(handle, owner_space, applied);
        return false;
      }
      else
      {
#ifdef DEBUG_LEGION
        assert(partition_ready.has_triggered());
#endif
        // Traverse down and destroy all of the child nodes
        // Need to make a copy of this in case the children
        // end up being deleted and removing themselves
        std::vector<IndexSpaceNode*> color_map_copy;
        {
          unsigned index = 0;
          AutoLock n_lock(node_lock,1,false/*exclusive*/);
          if (!color_map.empty())
          {
            color_map_copy.resize(color_map.size());
            for (std::map<LegionColor,IndexSpaceNode*>::const_iterator it =
                  color_map.begin(); it != color_map.end(); it++)
              color_map_copy[index++] = it->second;
          }
        }
        if (!color_map_copy.empty())
        {
          for (std::vector<IndexSpaceNode*>::const_iterator it = 
                color_map_copy.begin(); it != color_map_copy.end(); it++)
            if ((*it)->destroy_node(local_space, applied))
              delete (*it);
        }
        return remove_base_valid_ref(APPLICATION_REF, NULL/*mutator*/);
      }
    } 
#endif // defined(DEFINE_NT_TEMPLATES)

  }; // namespace Internal
}; // namespace Legion

